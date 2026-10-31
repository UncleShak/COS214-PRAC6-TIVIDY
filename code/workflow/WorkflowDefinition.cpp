#include "WorkflowDefinition.h"
#include "Stage.h"
#include "WorkItem.h"
#include "../routing/RoutingRule.h"
#include "../assignment/AssignmentRule.h"
#include <stdexcept>

WorkflowDefinition::WorkflowDefinition(WorkflowComponent* root, const std::string& version)
    : rootStage(root), definitionVersion(version), routingRule(nullptr), assignmentRule(nullptr) {
}

WorkflowDefinition::~WorkflowDefinition() {
    // The definition owns the blueprint tree. 
    // Deleting the root will cascade down if your Composite classes 
    // (Stage/TaskDecorator) delete their children in their destructors.
    delete rootStage;
     delete routingRule;
    delete assignmentRule;
    // Connectors belong to the application, so do not delete them here.
}

WorkflowComponent* WorkflowDefinition::getRootStage() const {
    return rootStage;
}

std::string WorkflowDefinition::getVersion() const {
    return definitionVersion;
}

const RoutingRule* WorkflowDefinition::getRoutingRule() const { return routingRule; }
AssignmentRule* WorkflowDefinition::getAssignmentRule() const { return assignmentRule; }
const DependencyGraph& WorkflowDefinition::getDependencies() const { return dependencies; }
const std::vector<std::string>& WorkflowDefinition::getEntryItems() const { return entryItems; }

const WorkflowDefinition::ConnectorBinding* WorkflowDefinition::getConnectorBinding(const std::string& item) const {
    std::map<std::string, ConnectorBinding>::const_iterator found = connectors.find(item);
    if (found == connectors.end()) return nullptr;
    return &found->second;
}

// Copy only structure and task descriptions, never a running task's state.
static WorkflowComponent* copyTree(WorkflowComponent* source) {
    Stage* stage = dynamic_cast<Stage*>(source);
    if (stage != nullptr) {
        Stage* copy = new Stage(stage->getName());
        try {
            const std::vector<WorkflowComponent*>& children = stage->getChildren();
            for (std::size_t i = 0; i < children.size(); i++) {
                WorkflowComponent* child = copyTree(children[i]);
                try { copy->add(child); }
                catch (...) { delete child; throw; }
            }
        } catch (...) {
            delete copy;
            throw;
        }
        return copy;
    }
    WorkItem* item = dynamic_cast<WorkItem*>(source);
    if (item != nullptr) return new WorkItem(item->getName(), item->getRequiredRole());
    throw std::logic_error("The blueprint must contain only stages and work items");
}

WorkflowComponent* WorkflowDefinition::createRuntimeTree() const {
    if (rootStage == nullptr) throw std::logic_error("Definition has no root stage");
    return copyTree(rootStage);
}
