#ifndef WORKFLOW_ENGINE_H
#define WORKFLOW_ENGINE_H

#include <string>
#include <map>
#include "../workflow/WorkflowInstance.h"

class WorkflowEngine {
private:
    std::map<std::string, WorkflowInstance*> activeInstances;

public:
    WorkflowEngine() = default;
    ~WorkflowEngine();
    
    // Facade operations to manage workflow execution
    WorkflowInstance* startInstance(const std::string& instanceId, WorkflowDefinition* def, DataPayload* data);
    void completeWorkItem(const std::string& instanceId, const std::string& workItemId);
    void assignWork(const std::string& instanceId, Participant* participant, const std::string& workItemId);
};

#endif