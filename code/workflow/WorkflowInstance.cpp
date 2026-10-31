// WorkflowInstance.cpp
#include "WorkflowInstance.h"
#include "WorkItem.h"
#include "Stage.h"
#include "../visitor/WorkflowVisitor.h"
#include "../participants/Participant.h"
#include "../integration/Connector.h"
#include "../data/KeyValuePayload.h"
#include <stdexcept>

namespace {
class ItemIndex : public WorkflowVisitor {
public:
    std::map<std::string, WorkItem*> items;

    void visitStage(Stage*) override {}

    void visitWorkItem(WorkItem* item) override {
        items[item->getName()] = item;
    }
};

void checkActor(WorkItem* item, Participant* actor) {
    if (actor == nullptr || item->getAssignedParticipant() != actor)
        throw std::logic_error(
            "Only the assigned participant may do this task"
        );
}
}

WorkflowInstance::WorkflowInstance(WorkflowDefinition* def, DataPayload* initialData) 
    : definition(def),
      root(nullptr),
      payload(nullptr),
      progress(nullptr) {
    // Instance isolation: clone the payload so changes don't affect other instances
    if (definition == nullptr ||
        definition->getRoutingRule() == nullptr)
        throw std::invalid_argument(
            "A configured definition is required"
        );

    root = definition->createRuntimeTree();

    payload = initialData == nullptr
        ? new KeyValuePayload()
        : initialData->clone();

    if (payload == nullptr) {
        delete root;
        throw std::runtime_error("Could not copy workflow data");
    }

    progress = new ProgressionTracker(
        *definition->getRoutingRule(),
        definition->getDependencies(),
        definition->getEntryItems()
    );

    ItemIndex index;
    root->accept(&index);
    items = index.items;
}

WorkflowInstance::~WorkflowInstance() {
    delete payload;
    delete progress;
    delete root;
    // Clean up current states map...
}

WorkItem* WorkflowInstance::getWorkItem(
    const std::string& name
) const {
    std::map<std::string, WorkItem*>::const_iterator found =
        items.find(name);

    if (found == items.end())
        throw std::out_of_range("Unknown work item: " + name);

    return found->second;
}

void WorkflowInstance::start() {
    std::vector<std::string> entries = progress->start();

    for (std::size_t i = 0; i < entries.size(); ++i)
        getWorkItem(entries[i])->makeAvailable();
}

bool WorkflowInstance::isComplete() const {
    return progress->isComplete();
}

bool WorkflowInstance::isStalled() const {
    return progress->isStalled();
}

const DataPayload* WorkflowInstance::getPayload() const {
    return payload;
}

const std::vector<std::string>&
WorkflowInstance::availableItems() const {
    return progress->availableItems();
}

void WorkflowInstance::assignWork(
    const std::string& name,
    Participant* person
) {
    WorkItem* item = getWorkItem(name);

    if (person == nullptr ||
        !person->hasRole(item->getRequiredRole()))
        throw std::logic_error(
            "Participant does not have the required role"
        );

    if (!progress->isAvailable(name) ||
        item->getCurrentState()->getStateName() != "Available")
        throw std::logic_error("Task is not available");

    item->assignTo(person);
    item->assign();
    person->workAssigned();
}

void WorkflowInstance::startWork(
    const std::string& name,
    Participant* actor
) {
    WorkItem* item = getWorkItem(name);
    checkActor(item, actor);
    item->start();
}

void WorkflowInstance::completeWork(
    const std::string& name,
    Participant* actor
) {
    WorkItem* item = getWorkItem(name);
    checkActor(item, actor);

    if (item->getCurrentState()->getStateName() != "Started")
        throw std::logic_error(
            "Task must be Started before completion"
        );

    // Run an external operation first. If it fails, the task
    // stays Started and the tracker stays unchanged.
    const WorkflowDefinition::ConnectorBinding* binding =
        definition->getConnectorBinding(name);

    DataPayload* newPayload = binding == nullptr
        ? payload->clone()
        : binding->connector->invoke(
            binding->operation,
            *payload
          ).release();

    if (newPayload == nullptr)
        throw std::runtime_error(
            "Could not get updated workflow data"
        );

    // Calculate the route using the returned payload.
    ProgressionResult result;

    try {
        result = progress->complete(name, *newPayload);
        item->request();
    } catch (...) {
        delete newPayload;
        throw;
    }

    delete payload;
    payload = newPayload;

    actor->workFinished();

    for (std::size_t i = 0;
         i < result.madeAvailable.size();
         ++i) {
        getWorkItem(result.madeAvailable[i])->makeAvailable();
    }
}

// Memento pattern: creating a snapshot for history and rollback
InstanceSnapshot* WorkflowInstance::createSnapshot() {
    // Create a deep copy of the current state map and payload
    std::map<std::string, WorkItemState*> stateCopies;
    for (auto const& pair : currentStates) {
        stateCopies[pair.first] = pair.second; // Assuming pointers are managed carefully
    }
    
    return new InstanceSnapshot(stateCopies, payload->clone());
}

void WorkflowInstance::restore(InstanceSnapshot* snapshot) {
    // Restore the states and payload from the memento
    this->currentStates = snapshot->getSavedStates();
    
    delete this->payload;
    this->payload = snapshot->getSavedPayload()->clone();
}