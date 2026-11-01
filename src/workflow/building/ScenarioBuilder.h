#ifndef TIVIDY_SCENARIO_BUILDER_H
#define TIVIDY_SCENARIO_BUILDER_H
#include "WorkflowBuilder.h"
#include "WorkflowDefinition.h"
#include "DataBasedRouting.h"
#include "AssignmentRule.h"
#include "Connector.h"
#include <memory>
#include <set>
class Stage;
/** @brief Shared assembly mechanics for concrete industry Builders.
 * Configure structure first, then rules, then connectors. No industry-specific logic.
 * All names are unique across the tree. Rules refer only to existing WorkItems.
 * A failed construction can always be restarted with reset/the Director.
 */
class ScenarioBuilder : public WorkflowBuilder {
public:
    ScenarioBuilder(const std::string& name, const std::string& version);
    void reset() override;
    std::unique_ptr<WorkflowDefinition> getResult() override;
protected:
    void buildStage(const std::string& name, const std::string& parent = "");
    void buildWorkItem(const std::string& name, const std::string& role, const std::string& stage);
    void buildEntry(const std::string& item);
    void buildDependency(const std::string& dependent, const std::string& prerequisite);
    void buildIdentityField(const std::string& field);
    void buildCancellationItem(const std::string& item);
    /// Route map only (for the interface): `items` form an exception route leaving the main
    /// line at `splitsFrom`. Does not change routing.
    void buildExceptionRoute(const std::string& splitsFrom, const std::vector<std::string>& items);
    void buildPath(const std::string& from, const std::string& to);
    void buildBranch(const std::string& from, const RoutingCondition&, const std::vector<std::string>& targets);
    void buildDefaultBranch(const std::string& from, const std::vector<std::string>& targets);
    /// Where `from` goes when it is rejected. Overrides its normal path or branches, in any order.
    void buildRejectionPath(const std::string& from, const std::vector<std::string>& targets);
    /// Completing `item` ends the run with `outcome` (e.g. "succeeded", "cancelled"). Tag in
    /// priority order: if several tagged items complete, the first tagged wins.
    void buildEndOutcome(const std::string& item, const std::string& outcome);
    void buildAssignmentRule(std::unique_ptr<AssignmentRule> rule);
    void buildConnector(const std::string& item, std::shared_ptr<Connector>, const std::string& operation);
    // An event type that cancels `cancel` and opens `open` (items reachable only this way count as reachable).
    // Proxy policy: only these roles may perform action ("assign", "start", "complete", "reject",
    // "cancel", "escalate") on any item of this workflow.
    void buildAccessRule(const std::string& action, const std::vector<std::string>& roles);
    // Decorator: the item must be completed within `duration` clock units of becoming available.
    void buildDeadline(const std::string& item, int duration);
    // Decorator: every completion attempt of the item is written to the instance history.
    void buildAudit(const std::string& item);
    // Chain of Responsibility: the first handler of this workflow's approval chain (owns the rest).
    void buildApprovalChain(std::unique_ptr<ApprovalHandler> chain);
    // The item is decided by the approval chain (see WorkflowDefinition::ApprovalRule).
    void buildApproval(const std::string& item, const std::string& valueField, const std::string& resultField);
    void buildInterrupt(const std::string& eventType, const std::vector<std::string>& cancel,
                        const std::vector<std::string>& open);
private:
    void requireProduct() const;
    void requireItem(const std::string&) const;
    void checkTargets(const std::string&, const std::vector<std::string>&) const;
    void recordEdges(const std::string&, const std::vector<std::string>&);
    std::string name, version;
    std::unique_ptr<WorkflowDefinition> product;
    DataBasedRouting* routing;
    std::map<std::string, Stage*> stages;
    std::set<std::string> items;
    std::set<std::string> defaults;
    std::map<std::string, std::set<std::string>> edges;
    std::map<std::string, std::vector<std::string>> rejectionEdges;   // kept apart: not a fixed path
};
#endif
