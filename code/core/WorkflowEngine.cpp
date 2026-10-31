#include "WorkflowEngine.h"

#include <sstream>
#include <stdexcept>

#include "../assignment/AssignmentRule.h"
#include "../core/WorkflowException.h"
#include "../participants/Participant.h"
#include "../participants/Role.h"
#include "../workflow/InstanceSnapshot.h"
#include "../workflow/WorkItem.h"
#include "../workflow/WorkflowDefinition.h"
#include "Event.h"
#include "../builder/EngineConfigurator.h"
#include "../scenarios/IndustryFactory.h"

WorkflowEngine::WorkflowEngine() : nextInstanceNumber(1) {}

WorkflowEngine::~WorkflowEngine() {
    // Instances refer to definitions and participants, so they go first.
    caretakers.clear();
    activeInstances.clear();
}

// ---- definitions ----

std::string WorkflowEngine::deployWorkflow(std::unique_ptr<WorkflowDefinition> definition) {
    if (!definition || definition->getRootStage() == nullptr) throw std::invalid_argument("Cannot deploy an empty definition");
    const std::string name = definition->getRootStage()->getName();
    if (definitions.count(name)) throw std::invalid_argument("Workflow '" + name + "' is already deployed");
    definitions[name] = std::move(definition);
    return name;
}

std::vector<std::string> WorkflowEngine::listDefinitions() const {
    std::vector<std::string> names;
    for (std::map<std::string, std::unique_ptr<WorkflowDefinition> >::const_iterator it = definitions.begin(); it != definitions.end(); ++it)
        names.push_back(it->first);
    return names;
}

const WorkflowDefinition* WorkflowEngine::getDefinition(const std::string& name) const {
    std::map<std::string, std::unique_ptr<WorkflowDefinition> >::const_iterator found = definitions.find(name);
    return found == definitions.end() ? nullptr : found->second.get();
}

std::string WorkflowEngine::installIndustry(const IndustryFactory& industry) {
    std::unique_ptr<WorkflowBuilder> builder = industry.createBuilder();
    if (!builder) throw std::invalid_argument("Industry '" + industry.getDisplayName() + "' made no builder");
    const std::string name = deployWorkflow(EngineConfigurator().constructScenario(*builder));
    std::vector<std::unique_ptr<Participant> > people = industry.createParticipants();
    for (std::size_t i = 0; i < people.size(); ++i) addParticipant(std::move(people[i]));
    return name;
}

// ---- participants ----

Participant* WorkflowEngine::addParticipant(const std::string& name, const std::string& id, const std::vector<std::string>& roles) {
    if (name.empty() || id.empty()) throw std::invalid_argument("A participant needs a name and an id");
    if (getParticipant(id) != nullptr) throw std::invalid_argument("Participant id '" + id + "' is already used");
    std::unique_ptr<Participant> person(new Participant(name, id));
    for (std::size_t i = 0; i < roles.size(); ++i) person->addRole(Role(roles[i]));
    participants.push_back(std::move(person));
    return participants.back().get();
}

Participant* WorkflowEngine::addParticipant(std::unique_ptr<Participant> person) {
    if (!person || person->getName().empty() || person->getID().empty())
        throw std::invalid_argument("A participant needs a name and an id");
    if (getParticipant(person->getID()) != nullptr)
        throw std::invalid_argument("Participant id '" + person->getID() + "' is already used");
    participants.push_back(std::move(person));
    return participants.back().get();
}

Participant* WorkflowEngine::getParticipant(const std::string& id) const {
    for (std::size_t i = 0; i < participants.size(); ++i)
        if (participants[i]->getID() == id) return participants[i].get();
    return nullptr;
}

std::vector<Participant*> WorkflowEngine::getParticipants() const {
    std::vector<Participant*> all;
    for (std::size_t i = 0; i < participants.size(); ++i) all.push_back(participants[i].get());
    return all;
}

Participant* WorkflowEngine::requireParticipant(const std::string& id) const {
    Participant* person = getParticipant(id);
    if (person == nullptr) throw std::invalid_argument("No participant with id '" + id + "'");
    return person;
}

// ---- instances ----

WorkflowInstance* WorkflowEngine::startInstance(const std::string& instanceId, const std::string& definitionName, const DataPayload* data) {
    const WorkflowDefinition* definition = getDefinition(definitionName);
    if (definition == nullptr) throw std::invalid_argument("No workflow called '" + definitionName + "' is deployed");
    std::string id = instanceId;
    if (id.empty()) {
        do {
            std::ostringstream generated;
            generated << definitionName << "-" << nextInstanceNumber++;
            id = generated.str();
        } while (activeInstances.count(id));
    }
    if (activeInstances.count(id)) throw std::invalid_argument("Instance with ID " + id + " already exists.");

    // Create and start the new isolated instance
    std::unique_ptr<WorkflowInstance> instance(new WorkflowInstance(id, definition, data, &clock));
    instance->start();
    WorkflowInstance* raw = instance.get();
    activeInstances[id] = std::move(instance);
    instanceOrder.push_back(id);
    caretakers[id].reset(new HistoryManager());
    return raw;
}

WorkflowInstance* WorkflowEngine::getInstance(const std::string& instanceId) const {
    std::map<std::string, std::unique_ptr<WorkflowInstance> >::const_iterator found = activeInstances.find(instanceId);
    return found == activeInstances.end() ? nullptr : found->second.get();
}

WorkflowInstance& WorkflowEngine::requireInstance(const std::string& instanceId) const {
    WorkflowInstance* instance = getInstance(instanceId);
    if (instance == nullptr) throw std::invalid_argument("Instance '" + instanceId + "' not found.");
    return *instance;
}

std::vector<std::string> WorkflowEngine::listInstances() const { return instanceOrder; }

std::vector<std::string> WorkflowEngine::getAvailableWork(const std::string& instanceId) const {
    return requireInstance(instanceId).getAvailableWork();
}

// ---- actions ----

void WorkflowEngine::perform(const std::string& instanceId, const std::string& label,
                             const std::function<void(WorkflowInstance&)>& action) {
    WorkflowInstance& instance = requireInstance(instanceId);
    std::unique_ptr<InstanceSnapshot> before(instance.createSnapshot());
    try {
        action(instance);
    } catch (...) {
        instance.restore(before.get());   // a failed action changes nothing
        recountWorkloads();
        throw;
    }
    caretakers[instanceId]->save(before.release(), label);
}

void WorkflowEngine::assignWork(const std::string& instanceId, const std::string& workItem,
                                const std::string& actorId, const std::string& assigneeId) {
    Participant* actor = requireParticipant(actorId);
    Participant* assignee = requireParticipant(assigneeId);
    const std::string role = requireInstance(instanceId).getItem(workItem)->getRequiredRole();
    if (!assignee->hasRole(role))
        throw AccessDenied(assignee->getName() + " does not have the role " + role + " needed for '" + workItem + "'");
    perform(instanceId, "assign " + workItem + " to " + assignee->getName(), [&](WorkflowInstance& instance) {
        instance.assignWork(workItem, actor, assignee);
    });
}

std::string WorkflowEngine::autoAssign(const std::string& instanceId, const std::string& workItem, const std::string& actorId) {
    WorkflowInstance& instance = requireInstance(instanceId);
    AssignmentRule* rule = instance.getDefinition()->getAssignmentRule();
    if (rule == nullptr) throw std::logic_error("Workflow has no assignment rule");
    Participant* chosen = rule->assignParticipant(instance.getItem(workItem), getParticipants());
    if (chosen == nullptr)
        throw std::logic_error("Nobody with the role " + instance.getItem(workItem)->getRequiredRole() + " can take '" + workItem + "'");
    assignWork(instanceId, workItem, actorId, chosen->getID());
    return chosen->getID();
}

void WorkflowEngine::startWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "start " + workItem, [&](WorkflowInstance& instance) { instance.startWork(workItem, actor); });
}

void WorkflowEngine::completeWorkItem(const std::string& instanceId, const std::string& workItem, const std::string& actorId) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "complete " + workItem, [&](WorkflowInstance& instance) { instance.completeWork(workItem, actor); });
}

void WorkflowEngine::rejectWork(const std::string& instanceId, const std::string& workItem,
                                const std::string& actorId, const std::string& reason) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "reject " + workItem, [&](WorkflowInstance& instance) { instance.rejectWork(workItem, actor, reason); });
}

void WorkflowEngine::cancelWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "cancel " + workItem, [&](WorkflowInstance& instance) { instance.cancelWork(workItem, actor); });
}

void WorkflowEngine::escalateWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "escalate " + workItem, [&](WorkflowInstance& instance) { instance.escalateWork(workItem, actor); });
}

void WorkflowEngine::setData(const std::string& instanceId, const std::string& key, const std::string& value) {
    perform(instanceId, "set " + key, [&](WorkflowInstance& instance) { instance.setData(key, value); });
}

void WorkflowEngine::raiseEvent(const std::string& instanceId, const std::string& type, const std::string& description) {
    if (type.empty()) throw std::invalid_argument("An event needs a type");
    perform(instanceId, "event " + type, [&](WorkflowInstance& instance) { instance.raise(Event(type, description)); });
}

// ---- approvals ----

void WorkflowEngine::approve(const std::string& instanceId, int requestId, const std::string& actorId, const std::string& note) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "approve APR-" + std::to_string(requestId), [&](WorkflowInstance& instance) {
        instance.approve(requestId, actor, note);
    });
}

void WorkflowEngine::rejectApproval(const std::string& instanceId, int requestId, const std::string& actorId,
                                    const std::string& reason) {
    Participant* actor = requireParticipant(actorId);
    perform(instanceId, "reject APR-" + std::to_string(requestId), [&](WorkflowInstance& instance) {
        instance.rejectApproval(requestId, actor, reason);
    });
}

// ---- time ----

std::vector<std::string> WorkflowEngine::advanceClock(int units) {
    if (units <= 0) throw std::invalid_argument("The clock only moves forward");
    clock.advance(units);
    std::vector<std::string> changes;
    for (std::size_t i = 0; i < instanceOrder.size(); ++i) {
        perform(instanceOrder[i], "clock +" + std::to_string(units), [&](WorkflowInstance& instance) {
            const std::vector<std::string> lines = instance.onClockAdvanced();
            changes.insert(changes.end(), lines.begin(), lines.end());
        });
    }
    recountWorkloads();
    return changes;
}

int WorkflowEngine::currentTime() const { return clock.now(); }

// ---- undo ----

bool WorkflowEngine::canUndo(const std::string& instanceId) const {
    requireInstance(instanceId);
    return caretakers.find(instanceId)->second->canUndo();
}

std::string WorkflowEngine::nextUndoLabel(const std::string& instanceId) const {
    requireInstance(instanceId);
    return caretakers.find(instanceId)->second->nextUndoLabel();
}

std::string WorkflowEngine::undo(const std::string& instanceId) {
    WorkflowInstance& instance = requireInstance(instanceId);
    const std::string label = caretakers[instanceId]->undo(instance);
    recountWorkloads();
    return label;
}

void WorkflowEngine::recountWorkloads() {
    std::map<Participant*, int> held;
    for (std::map<std::string, std::unique_ptr<WorkflowInstance> >::const_iterator it = activeInstances.begin(); it != activeInstances.end(); ++it) {
        const std::vector<WorkItem*> items = it->second->getItems();
        for (std::size_t i = 0; i < items.size(); ++i) {
            const std::string state = items[i]->getCurrentState()->getStateName();
            if (items[i]->getAssignedParticipant() != nullptr && (state == "Assigned" || state == "Started"))
                ++held[items[i]->getAssignedParticipant()];
        }
    }
    for (std::size_t i = 0; i < participants.size(); ++i) {
        Participant* person = participants[i].get();
        const int target = held[person];
        while (person->getWorkload() < target) person->workAssigned();
        while (person->getWorkload() > target) person->workFinished();
    }
}
