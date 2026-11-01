#ifndef WORKFLOW_DEFINITION_H
#define WORKFLOW_DEFINITION_H

// holds the current structure of a process and has now copies running
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "WorkflowComponent.h"
#include "../progression/DependencyGraph.h"
#include "../routing/RoutingRule.h"
#include "../assignment/AssignmentRule.h"
#include "../integration/Connector.h"
#include "../proxy/ActionAccessPolicy.h"
#include "../approval/ApprovalHandler.h"

class ScenarioBuilder;

class WorkflowDefinition {
    public:
        // Adapter binding: which external operation a work item calls
        struct ConnectorBinding {
            std::shared_ptr<Connector> connector;
            std::string operation;
        };

        // Observer reaction: when an event of this type is raised on an instance, the unfinished
        // items in `cancel` are cancelled (CancelCommand) and the items in `open` are reached.
        struct InterruptRule {
            std::string eventType;
            std::vector<std::string> cancel;
            std::vector<std::string> open;
        };

        // Decorator: extra behaviour wrapped around one item's work while it runs.
        struct DecorationRule {
            int deadline;   // DeadlineDecorator duration in clock units, -1 for none
            bool audit;     // AuditDecorator writing to the instance history
            DecorationRule() : deadline(-1), audit(false) {}
        };

        // Chain of Responsibility: the item is decided through the approval chain, not by
        // completing it directly. valueField holds the amount the chain judges; resultField
        // is set to "yes"/"no" when the decision is made, so routing can follow it.
        struct ApprovalRule {
            std::string valueField;
            std::string resultField;
        };

        // How a run ended: completing `item` gives the instance this outcome ("succeeded",
        // "cancelled", ...). Kept in setup order; when several tagged items completed, the
        // first one listed wins. See WorkflowInstance::getOutcome().
        struct EndOutcome {
            std::string item;
            std::string outcome;
        };

    private:
        WorkflowComponent* rootStage;
        std::string definitionVersion;

        // Filled in by a ScenarioBuilder (Builder pattern)
        std::vector<std::string> entryItems;
        DependencyGraph dependencies;
        std::unique_ptr<RoutingRule> routingRule;
        std::unique_ptr<AssignmentRule> assignmentRule;
        std::map<std::string, ConnectorBinding> connectors;
        std::vector<InterruptRule> interrupts;
        ActionAccessPolicy accessPolicy;   // Proxy: who may do what, for this organisation
        std::map<std::string, DecorationRule> decorations;
        std::map<std::string, ApprovalRule> approvals;
        std::unique_ptr<ApprovalHandler> approvalChain;   // first handler; owns its successors
        std::vector<EndOutcome> endOutcomes;
        friend class ScenarioBuilder;

        WorkflowDefinition(const WorkflowDefinition&);
        WorkflowDefinition& operator=(const WorkflowDefinition&);

    public:
        virtual ~WorkflowDefinition();
        WorkflowDefinition(WorkflowComponent* root, const std::string& version);
        virtual WorkflowComponent* getRootStage() const;
        virtual std::string getVersion() const;

        const std::vector<std::string>& getEntryItems() const;
        const DependencyGraph& getDependencies() const;
        RoutingRule* getRoutingRule() const;
        AssignmentRule* getAssignmentRule() const;
        // nullptr when the item has no connector
        const ConnectorBinding* getConnectorBinding(const std::string& itemName) const;
        const std::vector<InterruptRule>& getInterruptRules() const;
        const ActionAccessPolicy& getAccessPolicy() const;
        // nullptr when the item has none
        const DecorationRule* getDecoration(const std::string& itemName) const;
        const ApprovalRule* getApprovalRule(const std::string& itemName) const;
        ApprovalHandler* getApprovalChain() const;
        const std::vector<EndOutcome>& getEndOutcomes() const;
};

#endif
