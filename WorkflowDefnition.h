#ifndef WORKFLOW_DEFINITION_H
#define WORKFLOW_DEFINITION_H

#include "WorkflowComponent.h"

class WorkflowDefinition {
    public:
        virtual ~WorkflowDefinition() = default;
        virtual WorkflowComponent* getRootStage() const = 0;
        virtual std::string getVersion() const = 0;
};

#endif