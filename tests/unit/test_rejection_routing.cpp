// Envy: Bug 1 (demo-blocking): rejecting a step must take the rejection path, not stall the instance.
// Every rejectable step in both scenarios is driven to Started, rejected, and checked.
#include "doctest.h"

#include <memory>
#include <string>

#include "WorkflowEngine.h"
#include "ArkhamExpressFactory.h"
#include "DuneHarvestFactory.h"
#include "WorkItem.h"
#include "ApprovalRequest.h"
#include "RoutingCondition.h"
#include "KeyValuePayload.h"
#include "DataBasedRouting.h"

namespace {

struct Step { const char* item; const char* who; };

// Arkham's happy path, in order; whoever does the work is also who rejects it.
const Step ARKHAM[] = {
    {"ReceiveOrder", "ax-clerk"}, {"ValidateOrder", "ax-clerk"}, {"CheckStock", "ax-wh"},
    {"PreparePackage", "ax-wh"}, {"SelectDeliveryType", "ax-dm"}, {"AssignDriver", "ax-dm"},
    {"Dispatch", "ax-drv"}, {"Deliver", "ax-drv"}};
const int ARKHAM_STEPS = sizeof(ARKHAM) / sizeof(ARKHAM[0]);

struct World {
    WorkflowEngine engine;
    ArkhamExpressFactory arkham;
    DuneHarvestFactory dune;
    World() { engine.installIndustry(arkham); engine.installIndustry(dune); }

    void startArkham(const std::string& id) {
        std::unique_ptr<DataPayload> data = arkham.createInitialData(id);
        engine.startInstance(id, arkham.getWorkflowName(), data.get());
    }
    void startDune(const std::string& id) {
        std::unique_ptr<DataPayload> data = dune.createInitialData(id);
        engine.startInstance(id, dune.getWorkflowName(), data.get());
    }
    std::string state(const std::string& id, const std::string& item) {
        return engine.getInstance(id)->getItem(item)->getCurrentState()->getStateName();
    }
    std::string field(const std::string& id, const std::string& key) {
        const std::map<std::string, std::string> fields = engine.getInstance(id)->getDataFields();
        std::map<std::string, std::string>::const_iterator found = fields.find(key);
        return found == fields.end() ? "" : found->second;
    }
    void take(const std::string& id, const std::string& item, const std::string& who, const std::string& manager) {
        engine.assignWork(id, item, manager, who);
        engine.startWork(id, item, who);
    }
    void finish(const std::string& id, const std::string& item, const std::string& who, const std::string& manager) {
        take(id, item, who, manager);
        engine.completeWorkItem(id, item, who);
    }
    void throughPermit(const std::string& id) {
        finish(id, "SurveyField", "du-surveyor", "du-ops");
        take(id, "ApproveHarvestPermit", "du-ops", "du-ops");
        engine.approve(id, engine.getInstance(id)->getApprovals().back()->getId(), "du-ops", "quiet");
    }
};

}  // namespace

TEST_SUITE("envy: rejection routing") {

TEST_CASE("RoutingCondition::rejected reads the <item>.outcome field") {
    CHECK(RoutingCondition::outcomeField("Deliver") == "Deliver.outcome");
    KeyValuePayload data;
    CHECK_FALSE(RoutingCondition::rejected("Deliver").evaluate(data));
    data.set("Deliver.outcome", "completed");
    CHECK_FALSE(RoutingCondition::rejected("Deliver").evaluate(data));
    data.set("Deliver.outcome", "rejected");
    CHECK(RoutingCondition::rejected("Deliver").evaluate(data));
}

TEST_CASE("DataBasedRouting: a rejection path overrides the item's path and decision only when rejected") {
    DataBasedRouting routing;
    routing.addPath("Pack", "Ship");
    routing.addBranch("Check", RoutingCondition::equals("ok", "yes"), {"Pack"});
    routing.setDefaultBranch("Check", {"Retry"});
    routing.setRejectionPath("Pack", {"Cancel"});
    routing.setRejectionPath("Check", {"Cancel"});
    CHECK(routing.hasRejectionPath("Pack"));
    CHECK_FALSE(routing.hasRejectionPath("Ship"));
    CHECK_FALSE(routing.hasDecision("Pack"));   // still a fixed path, not a decision point

    KeyValuePayload data;
    data.set("ok", "yes");
    CHECK(routing.determineNextWork("Pack", data).description == "Sequence: Pack -> Ship");
    CHECK(routing.determineNextWork("Check", data).nextItems == std::vector<std::string>{"Pack"});

    data.set("Pack.outcome", "rejected");
    data.set("Check.outcome", "rejected");
    const RoutingDecision pack = routing.determineNextWork("Pack", data);
    CHECK(pack.nextItems == std::vector<std::string>{"Cancel"});
    CHECK(pack.description == "Rejected: Pack -> Cancel");
    CHECK(routing.determineNextWork("Check", data).nextItems == std::vector<std::string>{"Cancel"});   // beats ok=yes

    data.set("Pack.outcome", "completed");   // a retried item that later completes goes on as normal
    CHECK(routing.determineNextWork("Pack", data).nextItems == std::vector<std::string>{"Ship"});
    CHECK_THROWS_AS(routing.setRejectionPath("Pack", {}), std::invalid_argument);
}

TEST_CASE("Arkham: rejecting any step opens CancelOrder instead of stalling") {
    for (int reject = 0; reject < ARKHAM_STEPS; ++reject) {
        const std::string rejected = ARKHAM[reject].item; CAPTURE(rejected);
        World w;
        w.startArkham("AX-1");
        for (int i = 0; i < reject; ++i) w.finish("AX-1", ARKHAM[i].item, ARKHAM[i].who, "ax-dm");
        w.take("AX-1", ARKHAM[reject].item, ARKHAM[reject].who, "ax-dm");
        w.engine.rejectWork("AX-1", ARKHAM[reject].item, ARKHAM[reject].who, "test rejection");

        WorkflowInstance* instance = w.engine.getInstance("AX-1");
        CHECK(w.state("AX-1", ARKHAM[reject].item) == "Rejected");
        CHECK(w.field("AX-1", std::string(ARKHAM[reject].item) + ".outcome") == "rejected");
        CHECK(w.state("AX-1", "CancelOrder") == "Available");
        CHECK_FALSE(instance->isStalled());
        CHECK(w.state("AX-1", "CloseOrder") == "Created");   // the success path was not taken
    }
}

TEST_CASE("Arkham: a rejected sequential step can be cancelled out cleanly") {
    // Steps with no parallel sibling still open: CancelOrder is the only work left.
    const int sequential[] = {0, 1, 2, 6, 7};   // ReceiveOrder, ValidateOrder, CheckStock, Dispatch, Deliver
    for (int k = 0; k < 5; ++k) {
        const int reject = sequential[k];
        const std::string rejected = ARKHAM[reject].item; CAPTURE(rejected);
        World w;
        w.startArkham("AX-1");
        for (int i = 0; i < reject; ++i) w.finish("AX-1", ARKHAM[i].item, ARKHAM[i].who, "ax-dm");
        w.take("AX-1", ARKHAM[reject].item, ARKHAM[reject].who, "ax-dm");
        w.engine.rejectWork("AX-1", ARKHAM[reject].item, ARKHAM[reject].who, "test rejection");
        w.finish("AX-1", "CancelOrder", "ax-dm", "ax-dm");
        CHECK(w.engine.getInstance("AX-1")->isComplete());
    }
}

TEST_CASE("Arkham: completing a step records outcome=completed and keeps the success path") {
    World w;
    w.startArkham("AX-1");
    w.finish("AX-1", "ReceiveOrder", "ax-clerk", "ax-dm");
    CHECK(w.field("AX-1", "ReceiveOrder.outcome") == "completed");
    CHECK(w.state("AX-1", "ValidateOrder") == "Available");
    CHECK(w.state("AX-1", "CancelOrder") == "Created");
}

TEST_CASE("Arkham: undoing a rejection clears the outcome, and the step can then be completed") {
    World w;
    w.startArkham("AX-1");
    w.take("AX-1", "ReceiveOrder", "ax-clerk", "ax-dm");
    w.engine.rejectWork("AX-1", "ReceiveOrder", "ax-clerk", "typo");
    REQUIRE(w.state("AX-1", "CancelOrder") == "Available");

    w.engine.undo("AX-1");
    CHECK(w.state("AX-1", "ReceiveOrder") == "Started");
    CHECK(w.field("AX-1", "ReceiveOrder.outcome") == "");
    CHECK(w.state("AX-1", "CancelOrder") == "Created");

    w.engine.completeWorkItem("AX-1", "ReceiveOrder", "ax-clerk");
    CHECK(w.state("AX-1", "ValidateOrder") == "Available");
    CHECK(w.state("AX-1", "CancelOrder") == "Created");
}

TEST_CASE("Dune: rejecting the survey abandons the run") {
    World w;
    w.startDune("RUN-1");
    w.take("RUN-1", "SurveyField", "du-surveyor", "du-ops");
    w.engine.rejectWork("RUN-1", "SurveyField", "du-surveyor", "sandstorm");
    CHECK(w.state("RUN-1", "AbandonRun") == "Available");
    CHECK(w.state("RUN-1", "ApproveHarvestPermit") == "Created");
    w.finish("RUN-1", "AbandonRun", "du-ops", "du-ops");
    CHECK(w.engine.getInstance("RUN-1")->isComplete());
}

TEST_CASE("Dune: rejecting a deployment or the harvest abandons the run instead of stalling") {
    const Step steps[] = {{"DeployHarvester", "du-chief"}, {"DeployCarryall", "du-pilot"}, {"Harvest", "du-chief"}};
    for (int k = 0; k < 3; ++k) {
        const std::string rejected = steps[k].item; CAPTURE(rejected);
        World w;
        w.startDune("RUN-1");
        w.throughPermit("RUN-1");
        if (k == 2) {
            w.finish("RUN-1", "DeployHarvester", "du-chief", "du-ops");
            w.finish("RUN-1", "DeployCarryall", "du-pilot", "du-ops");
        }
        w.take("RUN-1", steps[k].item, steps[k].who, "du-ops");
        w.engine.rejectWork("RUN-1", steps[k].item, steps[k].who, "machine failure");
        CHECK(w.state("RUN-1", "AbandonRun") == "Available");
        CHECK_FALSE(w.engine.getInstance("RUN-1")->isStalled());
        CHECK(w.state("RUN-1", "TransportToRefinery") == "Created");
    }
}

TEST_CASE("Dune: a permit refused by the approval chain still abandons the run") {
    World w;
    w.startDune("RUN-1");
    w.finish("RUN-1", "SurveyField", "du-surveyor", "du-ops");
    w.take("RUN-1", "ApproveHarvestPermit", "du-ops", "du-ops");
    w.engine.rejectApproval("RUN-1", w.engine.getInstance("RUN-1")->getApprovals().back()->getId(), "du-ops", "storm");
    CHECK(w.field("RUN-1", "ApproveHarvestPermit.outcome") == "rejected");
    CHECK(w.state("RUN-1", "AbandonRun") == "Available");
    CHECK(w.state("RUN-1", "DeployHarvester") == "Created");
}

}  // TEST_SUITE
