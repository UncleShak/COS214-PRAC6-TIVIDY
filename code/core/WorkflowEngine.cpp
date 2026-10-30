#include "WorkflowEngine.h"
#include <iostream>
#include <stdexcept>

WorkflowEngine::~WorkflowEngine() {
    for (auto const& pair : activeInstances) {
        delete pair.second;
    }
    activeInstances.clear();
}

WorkflowInstance* WorkflowEngine::startInstance(const std::string& instanceId, WorkflowDefinition* def, DataPayload* data) {
    if (activeInstances.find(instanceId) != activeInstances.end()) {
        throw std::logic_error("Instance with ID " + instanceId + " already exists.");
    }
    
    // Create and start the new isolated instance
    WorkflowInstance* newInstance = new WorkflowInstance(instanceId, def, data);
    newInstance->start();
    
    activeInstances[instanceId] = newInstance;
    return newInstance;
}

void WorkflowEngine::completeWorkItem(const std::string& instanceId, const std::string& workItemId) {
    auto it = activeInstances.find(instanceId);
    if (it == activeInstances.end()) {
        throw std::logic_error("Instance not found.");
    }
    
    // In the full implementation, you fetch the specific WorkItem from the instance using an Iterator
    // and call complete() on it, which triggers the State pattern transitions.
    std::cout << "Executing complete transition for work item: " << workItemId << " in instance: " << instanceId << "\n";
}

void WorkflowEngine::assignWork(const std::string& instanceId, Participant* participant, const std::string& workItemId) {
    auto it = activeInstances.find(instanceId);
    if (it == activeInstances.end()) {
        throw std::logic_error("Instance not found.");
    }
    
    // Similarly, locate the WorkItem and execute the assign transition.
    std::cout << "Assigning work item: " << workItemId << " to participant in instance: " << instanceId << "\n";
}