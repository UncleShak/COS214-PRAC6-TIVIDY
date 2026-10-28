#include "WorkflowDefinition.h"

WorkflowDefinition::WorkflowDefinition(WorkflowComponent* root, const std::string& version)
    : rootStage(root), definitionVersion(version) {
}

WorkflowDefinition::~WorkflowDefinition() {
    // The definition owns the blueprint tree. 
    // Deleting the root will cascade down if your Composite classes 
    // (Stage/TaskDecorator) delete their children in their destructors.
    delete rootStage;
}

WorkflowComponent* WorkflowDefinition::getRootStage() const {
    return rootStage;
}

std::string WorkflowDefinition::getVersion() const {
    return definitionVersion;
}