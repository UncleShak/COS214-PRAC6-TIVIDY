// Envy: other-bugs #17. isComplete() can't tell success from abandonment. The setup tags its end
// steps with an outcome (buildEndOutcome); WorkflowInstance::getOutcome() reports how a run ended.
#include "doctest.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "WorkflowEngine.h"
#include "ArkhamExpressFactory.h"
#include "DuneHarvestFactory.h"
#include "WorkItem.h"
#include "ApprovalRequest.h"
#include "ScenarioBuilder.h"

namespace {

struct World {
    WorkflowEngine engine;
    ArkhamExpressFactory arkham;
    DuneHarvestFactory dune;
    World() { engine.installIndustry(arkham); engine.installIndustry(dune); }

    WorkflowInstance* instance(const std::string& id) { return engine.getInstance(id); }
    std::string outcome(const std::string& id) { return instance(id)->getOutcome(); }
    void finish(const std::string& id, const std::string& item, const std::string& who, const std::string& manager) {
        engine.assignWork(id, item, manager, who);
        engine.startWork(id, item, who);
        engine.completeWorkItem(id, item, who);
    }
    void startArkham(const std::string& id) {
        std::unique_ptr<DataPayload> data = arkham.createInitialData(id);
        engine.startInstance(id, arkham.getWorkflowName(), data.get());
    }
    void arkhamToDeliver(const std::string& id) {
        startArkham(id);
        finish(id, "ReceiveOrder", "ax-clerk", "ax-dm");
        finish(id, "ValidateOrder", "ax-clerk", "ax-dm");
        finish(id, "CheckStock", "ax-wh", "ax-dm");
        finish(id, "PreparePackage", "ax-wh", "ax-dm");
        finish(id, "SelectDeliveryType", "ax-dm", "ax-dm");
        finish(id, "AssignDriver", "ax-dm", "ax-dm");
        finish(id, "Dispatch", "ax-drv", "ax-dm");
    }
    void startDune(const std::string& id) {
        std::unique_ptr<DataPayload> data = dune.createInitialData(id);
        engine.startInstance(id, dune.getWorkflowName(), data.get());
    }
    void duneThroughHarvest(const std::string& id) {
        startDune(id);
        finish(id, "SurveyField", "du-surveyor", "du-ops");
        engine.assignWork(id, "ApproveHarvestPermit", "du-ops", "du-ops");
        engine.startWork(id, "ApproveHarvestPermit", "du-ops");
        engine.approve(id, instance(id)->getApprovals().back()->getId(), "du-ops", "quiet");
        finish(id, "DeployHarvester", "du-chief", "du-ops");
        finish(id, "DeployCarryall", "du-pilot", "du-ops");
        finish(id, "Harvest", "du-chief", "du-ops");
    }
};

}  // namespace

TEST_SUITE("envy: instance outcome") {

TEST_CASE("Arkham: a delivered order succeeds; the outcome says running until then") {
    World w;
    w.arkhamToDeliver("AX-1");
    CHECK(w.outcome("AX-1") == "running");
    w.finish("AX-1", "Deliver", "ax-drv", "ax-dm");
    CHECK(w.outcome("AX-1") == "running");          // CloseOrder still open
    w.finish("AX-1", "CloseOrder", "ax-clerk", "ax-dm");
    CHECK(w.instance("AX-1")->isComplete());
    CHECK(w.outcome("AX-1") == "succeeded");
}

TEST_CASE("Arkham: a rejected step, an invalid order and a customer cancellation all end cancelled") {
    World w;
    w.startArkham("AX-1");
    w.engine.assignWork("AX-1", "ReceiveOrder", "ax-dm", "ax-clerk");
    w.engine.startWork("AX-1", "ReceiveOrder", "ax-clerk");
    w.engine.rejectWork("AX-1", "ReceiveOrder", "ax-clerk", "bad address");
    w.finish("AX-1", "CancelOrder", "ax-dm", "ax-dm");
    CHECK(w.outcome("AX-1") == "cancelled");

    w.startArkham("AX-2");
    w.engine.setData("AX-2", "orderValid", "no");
    w.finish("AX-2", "ReceiveOrder", "ax-clerk", "ax-dm");
    w.finish("AX-2", "ValidateOrder", "ax-clerk", "ax-dm");
    w.finish("AX-2", "CancelOrder", "ax-dm", "ax-dm");
    CHECK(w.outcome("AX-2") == "cancelled");

    w.startArkham("AX-3");
    w.finish("AX-3", "ReceiveOrder", "ax-clerk", "ax-dm");
    w.engine.raiseEvent("AX-3", "CUSTOMER_CANCELLED", "changed mind");
    w.finish("AX-3", "CancelOrder", "ax-dm", "ax-dm");
    CHECK(w.instance("AX-3")->isComplete());
    CHECK(w.outcome("AX-3") == "cancelled");
}

TEST_CASE("Arkham: an undelivered parcel ends returned") {
    World w;
    w.arkhamToDeliver("AX-1");
    w.engine.setData("AX-1", "delivered", "no");
    w.finish("AX-1", "Deliver", "ax-drv", "ax-dm");
    w.finish("AX-1", "ReturnToWarehouse", "ax-drv", "ax-dm");
    CHECK(w.instance("AX-1")->isComplete());
    CHECK(w.outcome("AX-1") == "returned");
}

TEST_CASE("Arkham: work that stops without reaching an end step is 'stopped', not success") {
    World w;
    w.startArkham("AX-1");
    w.finish("AX-1", "ReceiveOrder", "ax-clerk", "ax-dm");
    w.finish("AX-1", "ValidateOrder", "ax-clerk", "ax-dm");
    w.finish("AX-1", "CheckStock", "ax-wh", "ax-dm");
    w.finish("AX-1", "PreparePackage", "ax-wh", "ax-dm");
    w.engine.cancelWork("AX-1", "SelectDeliveryType", "ax-dm");
    w.finish("AX-1", "AssignDriver", "ax-dm", "ax-dm");
    CHECK(w.instance("AX-1")->isComplete());
    CHECK(w.outcome("AX-1") == "stopped");
}

TEST_CASE("Dune: shipped spice succeeds, a refused permit abandons, a worm sign evacuates") {
    World w;
    w.duneThroughHarvest("RUN-1");
    w.finish("RUN-1", "TransportToRefinery", "du-pilot", "du-ops");
    CHECK(w.outcome("RUN-1") == "succeeded");

    w.startDune("RUN-2");
    w.finish("RUN-2", "SurveyField", "du-surveyor", "du-ops");
    w.engine.assignWork("RUN-2", "ApproveHarvestPermit", "du-ops", "du-ops");
    w.engine.startWork("RUN-2", "ApproveHarvestPermit", "du-ops");
    w.engine.rejectApproval("RUN-2", w.instance("RUN-2")->getApprovals().back()->getId(), "du-ops", "storm");
    w.finish("RUN-2", "AbandonRun", "du-ops", "du-ops");
    CHECK(w.outcome("RUN-2") == "abandoned");

    // Worm sign after the harvest: the spice still ships, but the run is remembered as evacuated.
    w.duneThroughHarvest("RUN-3");
    w.engine.raiseEvent("RUN-3", "WORM_SIGN", "wormsign!");
    w.finish("RUN-3", "Evacuate", "du-pilot", "du-ops");
    if (!w.instance("RUN-3")->isComplete()) w.finish("RUN-3", "TransportToRefinery", "du-pilot", "du-ops");
    CHECK(w.instance("RUN-3")->isComplete());
    CHECK(w.outcome("RUN-3") == "evacuated");
}

TEST_CASE("Undo takes the outcome back with it") {
    World w;
    w.duneThroughHarvest("RUN-1");
    w.finish("RUN-1", "TransportToRefinery", "du-pilot", "du-ops");
    REQUIRE(w.outcome("RUN-1") == "succeeded");
    w.engine.undo("RUN-1");
    CHECK(w.outcome("RUN-1") == "running");
}

TEST_CASE("Builder: end outcomes are checked when the setup is built") {
    // Reserved words would make a finished run look unfinished, so they are refused.
    struct Tiny : ScenarioBuilder {
        Tiny() : ScenarioBuilder("Tiny", "1") {}
        void buildStructure() override { buildStage("S"); buildWorkItem("A", "R", "S"); buildWorkItem("B", "R", "S"); }
        void buildRules() override {}
        void buildConnectors() override {}
        void tag(const std::string& item, const std::string& outcome) { buildEndOutcome(item, outcome); }
    };
    Tiny t;
    t.reset();
    t.buildStructure();
    CHECK_THROWS_AS(t.tag("B", "running"), std::invalid_argument);
    CHECK_THROWS_AS(t.tag("B", "stopped"), std::invalid_argument);
    CHECK_THROWS_AS(t.tag("B", ""), std::invalid_argument);
    CHECK_THROWS_AS(t.tag("Nope", "done"), std::invalid_argument);
    t.tag("B", "done");
    CHECK_THROWS_AS(t.tag("B", "other"), std::invalid_argument);   // one outcome per step
}

}  // TEST_SUITE
