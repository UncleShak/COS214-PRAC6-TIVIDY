#ifndef WORKFLOW_ENGINE_H
#define WORKFLOW_ENGINE_H

#include <string>
#include "WorkflowInstance.h"

class WorkflowEngine {
public:
    virtual ~WorkflowEngine() = default;
    
    // Facade methods
    virtual WorkflowInstance* startInstance(const std::string& definitionName) = 0;
    virtual void completeWorkItem(const std::string& instanceId, const std::string& workItemId) = 0;
    virtual void assignWork(const std::string& instanceId) = 0;
};

#endif