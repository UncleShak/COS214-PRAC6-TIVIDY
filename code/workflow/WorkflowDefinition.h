#ifndef WORKFLOW_DEFINITION_H
#define WORKFLOW_DEFINITION_H

// holds the current structure of a process and has now copies running
#include <map>
#include <string>
#include <vector>
#include "WorkflowComponent.h"
#include "../progression/DependencyGraph.h"

class RoutingRule;
class AssignmentRule;
class Connector;
class ScenarioBuilder;

class WorkflowDefinition {
    private:
        friend class ScenarioBuilder;
        WorkflowComponent* rootStage;
        std::string definitionVersion;
        RoutingRule* routingRule; // Owned.
        AssignmentRule* assignmentRule; // Owned.
        DependencyGraph dependencies;
        std::vector<std::string> entryItems;
        std::map<std::string, ConnectorBinding> connectors;

        // Prevent two definitions from deleting the same owned pointers.
    WorkflowDefinition(const WorkflowDefinition&);
    WorkflowDefinition& operator=(const WorkflowDefinition&);

    public:
    struct ConnectorBinding {
        Connector* connector; // Borrowed: the application owns this connector.
        std::string operation;
    };

        virtual ~WorkflowDefinition();
        WorkflowDefinition(WorkflowComponent* root, const std::string& version);
        virtual WorkflowComponent* getRootStage() const;
        virtual std::string getVersion() const;

        const RoutingRule* getRoutingRule() const;
    AssignmentRule* getAssignmentRule() const;
    const DependencyGraph& getDependencies() const;
    const std::vector<std::string>& getEntryItems() const;
    const ConnectorBinding* getConnectorBinding(const std::string& item) const;

    // Returns a new tree. The caller must delete it when its instance finishes.
    WorkflowComponent* createRuntimeTree() const;
};

#endif