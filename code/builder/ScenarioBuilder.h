#ifndef TIVIDY_SCENARIO_BUILDER_H
#define TIVIDY_SCENARIO_BUILDER_H
#include "WorkflowBuilder.h"
#include "../workflow/WorkflowDefinition.h"
#include "../routing/DataBasedRouting.h"
#include "../assignment/AssignmentRule.h"
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
    void buildPath(const std::string& from, const std::string& to);
    void buildBranch(const std::string& from, const RoutingCondition&, const std::vector<std::string>& targets);
    void buildDefaultBranch(const std::string& from, const std::vector<std::string>& targets);
    void buildAssignmentRule(std::unique_ptr<AssignmentRule> rule);
    void buildConnector(const std::string& item, std::shared_ptr<Connector>, const std::string& operation);
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
};
#endif
