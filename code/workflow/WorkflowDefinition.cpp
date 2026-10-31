#include "WorkflowDefinition.h"
#include "Stage.h"

WorkflowDefinition::WorkflowDefinition(WorkflowComponent* root, const std::string& version)
    : rootStage(root), definitionVersion(version) {
}

WorkflowDefinition::~WorkflowDefinition() {
    // Stages do not own their children, so the definition frees the whole tree it holds.
    deleteComponentTree(rootStage);
}

WorkflowComponent* WorkflowDefinition::getRootStage() const {
    return rootStage;
}

std::string WorkflowDefinition::getVersion() const {
    return definitionVersion;
}

const std::vector<std::string>& WorkflowDefinition::getEntryItems() const { return entryItems; }
const DependencyGraph& WorkflowDefinition::getDependencies() const { return dependencies; }
RoutingRule* WorkflowDefinition::getRoutingRule() const { return routingRule.get(); }
AssignmentRule* WorkflowDefinition::getAssignmentRule() const { return assignmentRule.get(); }

const ActionAccessPolicy& WorkflowDefinition::getAccessPolicy() const { return accessPolicy; }

const WorkflowDefinition::DecorationRule* WorkflowDefinition::getDecoration(const std::string& itemName) const {
    std::map<std::string, DecorationRule>::const_iterator found = decorations.find(itemName);
    return found == decorations.end() ? nullptr : &found->second;
}

const WorkflowDefinition::ApprovalRule* WorkflowDefinition::getApprovalRule(const std::string& itemName) const {
    std::map<std::string, ApprovalRule>::const_iterator found = approvals.find(itemName);
    return found == approvals.end() ? nullptr : &found->second;
}

ApprovalHandler* WorkflowDefinition::getApprovalChain() const { return approvalChain.get(); }

const std::vector<WorkflowDefinition::InterruptRule>& WorkflowDefinition::getInterruptRules() const {
    return interrupts;
}

const WorkflowDefinition::ConnectorBinding* WorkflowDefinition::getConnectorBinding(const std::string& itemName) const {
    std::map<std::string, ConnectorBinding>::const_iterator found = connectors.find(itemName);
    return found == connectors.end() ? nullptr : &found->second;
}
