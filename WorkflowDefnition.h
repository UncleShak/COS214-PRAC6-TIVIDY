#ifndef WORKFLOW_DEFINITION_H
#define WORKFLOW_DEFINITION_H

// holds the current structure of a process and has now copies running

#include "WorkflowComponent.h"

class WorkflowDefinition {
    public:
        virtual ~WorkflowDefinition() = default;
        virtual WorkflowComponent* getRootStage() const = 0;
        virtual std::string getVersion() const = 0;
};

#endif