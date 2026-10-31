// Integration: Proxy (protection) guards every participant action in the running engine.
// Commands are the clients; the policy comes from the WorkflowDefinition.
#include "doctest.h"

#include <memory>
#include <stdexcept>

#include "builder/EngineConfigurator.h"
#include "builder/ScenarioBuilder.h"
#include "core/WorkflowEngine.h"
#include "core/WorkflowException.h"
#include "assignment/RoleBasedAssignment.h"
#include "proxy/AccessControlProxy.h"
#include "proxy/ActionAccessPolicy.h"
#include "participants/Participant.h"
#include "workflow/WorkItem.h"

namespace {

std::string stateOf(WorkflowEngine& engine, const std::string& item) {
    return engine.getInstance("r1")->getItem(item)->getCurrentState()->getStateName();
}

// Survey -> Harvest. Only an OperationsManager may cancel; WORM cancels Harvest.
class GuardedBuilder : public ScenarioBuilder {
public:
    GuardedBuilder() : ScenarioBuilder("Guarded", "1") {}
    void buildStructure() override {
        buildStage("Run");
        buildWorkItem("Survey", "Surveyor", "Run");
        buildWorkItem("Harvest", "CrewChief", "Run");
        buildWorkItem("Evacuate", "CrewChief", "Run");
    }
    void buildRules() override {
        buildEntry("Survey");
        buildPath("Survey", "Harvest");
        buildAccessRule("cancel", {"OperationsManager"});
        buildInterrupt("WORM", {"Harvest"}, {"Evacuate"});
        buildAssignmentRule(std::unique_ptr<AssignmentRule>(new RoleBasedAssignment()));
    }
    void buildConnectors() override {}
    using ScenarioBuilder::buildAccessRule;
};

struct Guarded {
    WorkflowEngine engine;
    Guarded() {
        GuardedBuilder builder;
        engine.deployWorkflow(EngineConfigurator().constructScenario(builder));
        engine.addParticipant("Stilgar", "sur", {"Surveyor"});
        engine.addParticipant("Gurney", "crew", {"CrewChief"});
        engine.addParticipant("Jessica", "ops", {"OperationsManager", "Manager"});
        engine.startInstance("r1", "Guarded", nullptr);
    }
};

}  // namespace

TEST_SUITE("integration") {

TEST_CASE("Proxy: a restricted action is refused for other roles and nothing changes") {
    Guarded g;
    CHECK_THROWS_AS(g.engine.cancelWork("r1", "Survey", "sur"), AccessDenied);
    CHECK(stateOf(g.engine, "Survey") == "Available");
    CHECK_FALSE(g.engine.canUndo("r1"));
    g.engine.cancelWork("r1", "Survey", "ops");
    CHECK(stateOf(g.engine, "Survey") == "Cancelled");
}

TEST_CASE("Proxy: unrestricted actions need the item's role or a manager override") {
    Guarded g;
    CHECK_THROWS_AS(g.engine.assignWork("r1", "Survey", "crew", "sur"), AccessDenied);   // crew chief can't hand out survey work
    g.engine.assignWork("r1", "Survey", "sur", "sur");                                    // surveyor takes it
    g.engine.undo("r1");
    g.engine.assignWork("r1", "Survey", "ops", "sur");                                    // a manager may assign anything
    CHECK(stateOf(g.engine, "Survey") == "Assigned");
}

TEST_CASE("Proxy: refused before the State pattern or the item is reached") {
    Guarded g;
    g.engine.assignWork("r1", "Survey", "sur", "sur");
    g.engine.startWork("r1", "Survey", "sur");
    // A crew chief completing someone else's survey: the proxy refuses it, not the assignee rule.
    CHECK_THROWS_AS(g.engine.completeWorkItem("r1", "Survey", "crew"), AccessDenied);
    CHECK(stateOf(g.engine, "Survey") == "Started");
}

TEST_CASE("Proxy: automatic actions (interrupts) are not participant actions and are not guarded") {
    Guarded g;
    g.engine.assignWork("r1", "Survey", "sur", "sur");
    g.engine.startWork("r1", "Survey", "sur");
    g.engine.completeWorkItem("r1", "Survey", "sur");
    g.engine.raiseEvent("r1", "WORM", "");
    CHECK(stateOf(g.engine, "Harvest") == "Cancelled");
    CHECK(stateOf(g.engine, "Evacuate") == "Available");
}

TEST_CASE("ActionAccessPolicy: restricted actions use their list, others fall back to roles") {
    ActionAccessPolicy policy;
    policy.restrict("cancel", {"OperationsManager"});
    CHECK(policy.isAllowed("OperationsManager", "CrewChief", "cancel"));
    CHECK_FALSE(policy.isAllowed("Manager", "CrewChief", "cancel"));   // no override on a restricted action
    CHECK(policy.isAllowed("CrewChief", "CrewChief", "start"));
    CHECK(policy.isAllowed("Manager", "CrewChief", "start"));
    CHECK_FALSE(policy.isAllowed("Surveyor", "CrewChief", "start"));
    CHECK(policy.rolesFor("cancel") == std::vector<std::string>{"OperationsManager"});
    CHECK_THROWS_AS(policy.restrict("", {"X"}), std::invalid_argument);
}

TEST_CASE("AccessControlProxy with a participant checks every one of their roles") {
    WorkItem item("Harvest", "CrewChief");
    ActionAccessPolicy policy;
    Participant both("Duncan", "d");
    both.addRole(Role("Surveyor"));
    both.addRole(Role("CrewChief"));
    Participant neither("Feyd", "f");
    neither.addRole(Role("Surveyor"));
    AccessControlProxy allowed(&item, &policy, both, "CrewChief");
    AccessControlProxy refused(&item, &policy, neither, "CrewChief");
    CHECK_NOTHROW(allowed.authorize("start"));
    CHECK_THROWS_AS(refused.authorize("start"), AccessDenied);
    CHECK_THROWS_AS(refused.request(), AccessDenied);
    CHECK(item.getCurrentState()->getStateName() == "Created");   // the real subject was never reached
}

TEST_CASE("Builder rejects access rules for unknown actions") {
    GuardedBuilder builder;
    builder.reset();
    builder.buildStructure();
    CHECK_THROWS_AS(builder.buildAccessRule("teleport", {"OperationsManager"}), std::invalid_argument);
}

}  // TEST_SUITE
