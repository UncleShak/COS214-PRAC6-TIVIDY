// WorkflowInstance.cpp
#include "WorkflowInstance.h"
#include "InstanceSnapshot.h"

WorkflowInstance::WorkflowInstance(WorkflowDefinition* def, DataPayload* initialData) 
    : definition(def) {
    // Instance isolation: clone the payload so changes don't affect other instances
    if (initialData != nullptr) {
        payload = initialData->clone();
    } else {
        payload = nullptr;
    }
}

WorkflowInstance::~WorkflowInstance() {
    delete payload;
    // Clean up current states map...
}

void WorkflowInstance::start() {
    // Logic to find the root stage from the definition and make its first tasks Available
}

bool WorkflowInstance::isComplete() const {
    // Check if the workflow has reached a final state
    return false; 
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