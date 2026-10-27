#ifndef WORKFLOW_DEFINITION_H
#define WORKFLOW_DEFINITION_H

// holds the current structure of a process and has now copies running
#include <string>
#include "WorkflowComponent.h"

class WorkflowDefinition {
    private:
        WorkflowComponent* rootStage;
        std::string definitionVersion;

    public:
        virtual ~WorkflowDefinition();
        WorkflowDefinition(WorkflowComponent* root, const std::string& version);
        virtual WorkflowComponent* getRootStage() const;
        virtual std::string getVersion() const;
};

#endif