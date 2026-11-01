// Envy: other-bugs #1. Cancelling (or rejecting) one of two parallel steps must not leave the
// step that joins them waiting forever. A step whose prerequisite was cancelled or rejected can
// never start, so the tracker drops it (counts it cancelled) instead of keeping it waiting.
#include "doctest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "WorkflowEngine.h"
#include "ArkhamExpressFactory.h"
#include "DuneHarvestFactory.h"
#include "WorkItem.h"
#include "ApprovalRequest.h"
#include "DataBasedRouting.h"
#include "DependencyGraph.h"
#include "KeyValuePayload.h"
#include "ProgressionTracker.h"

namespace {

bool has(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

struct World {
    WorkflowEngine engine;
    ArkhamExpressFactory arkham;
    DuneHarvestFactory dune;
    World() { engine.installIndustry(arkham); engine.installIndustry(dune); }

    WorkflowInstance* instance(const std::string& id) { return engine.getInstance(id); }
    const ProgressionTracker& progress(const std::string& id) { return instance(id)->getProgress(); }
    void finish(const std::string& id, const std::string& item, const std::string& who, const std::string& manager) {
        engine.assignWork(id, item, manager, who);
        engine.startWork(id, item, who);
        engine.completeWorkItem(id, item, who);
    }
    /// Arkham order up to the parallel split: SelectDeliveryType and AssignDriver are Available.
    void arkhamToSplit(const std::string& id) {
        std::unique_ptr<DataPayload> data = arkham.createInitialData(id);
        engine.startInstance(id, arkham.getWorkflowName(), data.get());
        finish(id, "ReceiveOrder", "ax-clerk", "ax-dm");
        finish(id, "ValidateOrder", "ax-clerk", "ax-dm");
        finish(id, "CheckStock", "ax-wh", "ax-dm");
        finish(id, "PreparePackage", "ax-wh", "ax-dm");
    }
    /// Dune run through the permit: DeployHarvester and DeployCarryall are Available.
    void duneToSplit(const std::string& id) {
        std::unique_ptr<DataPayload> data = dune.createInitialData(id);
        engine.startInstance(id, dune.getWorkflowName(), data.get());
        finish(id, "SurveyField", "du-surveyor", "du-ops");
        engine.assignWork(id, "ApproveHarvestPermit", "du-ops", "du-ops");
        engine.startWork(id, "ApproveHarvestPermit", "du-ops");
        engine.approve(id, instance(id)->getApprovals().back()->getId(), "du-ops", "quiet");
    }
};

}  // namespace

TEST_SUITE("envy: cancelled prerequisites") {

TEST_CASE("Arkham: DeliveryManager cancels one delivery step, the other completes, nothing waits") {
    const char* const cancelFirst[] = {"SelectDeliveryType", "AssignDriver"};
    for (int k = 0; k < 2; ++k) {
        const std::string cancelled = cancelFirst[k];
        const std::string other = cancelFirst[1 - k];
        CAPTURE(cancelled);
        World w;
        w.arkhamToSplit("AX-1");
        w.engine.cancelWork("AX-1", cancelled, "ax-dm");
        w.finish("AX-1", other, "ax-dm", "ax-dm");

        CHECK_FALSE(w.progress("AX-1").isWaiting("Dispatch"));
        CHECK(w.progress("AX-1").isCancelled("Dispatch"));
        CHECK_FALSE(w.instance("AX-1")->isStalled());
        CHECK(w.instance("AX-1")->isComplete());
    }
}

TEST_CASE("Arkham: cancelling a prerequisite drops a step that was already waiting on it") {
    World w;
    w.arkhamToSplit("AX-1");
    w.finish("AX-1", "SelectDeliveryType", "ax-dm", "ax-dm");
    REQUIRE(w.progress("AX-1").isWaiting("Dispatch"));    // waiting for AssignDriver

    w.engine.cancelWork("AX-1", "AssignDriver", "ax-dm");
    CHECK_FALSE(w.progress("AX-1").isWaiting("Dispatch"));
    CHECK(w.progress("AX-1").isCancelled("Dispatch"));
    CHECK(w.instance("AX-1")->isComplete());

    // Undo puts the cancel back, and Dispatch is waiting again.
    w.engine.undo("AX-1");
    CHECK(w.progress("AX-1").isAvailable("AssignDriver"));
    CHECK(w.progress("AX-1").isWaiting("Dispatch"));
    w.finish("AX-1", "AssignDriver", "ax-dm", "ax-dm");
    CHECK(w.progress("AX-1").isAvailable("Dispatch"));
}

TEST_CASE("Arkham: rejecting one delivery step opens CancelOrder and the other step can't stall it") {
    World w;
    w.arkhamToSplit("AX-1");
    w.engine.assignWork("AX-1", "SelectDeliveryType", "ax-dm", "ax-dm");
    w.engine.startWork("AX-1", "SelectDeliveryType", "ax-dm");
    w.engine.rejectWork("AX-1", "SelectDeliveryType", "ax-dm", "no courier");
    REQUIRE(w.progress("AX-1").isAvailable("CancelOrder"));

    w.finish("AX-1", "AssignDriver", "ax-dm", "ax-dm");
    CHECK_FALSE(w.progress("AX-1").isWaiting("Dispatch"));
    w.finish("AX-1", "CancelOrder", "ax-dm", "ax-dm");
    CHECK(w.instance("AX-1")->isComplete());
}

TEST_CASE("Dune: OperationsManager cancels one deployment; Harvest never waits forever") {
    const char* const deployments[] = {"DeployHarvester", "DeployCarryall"};
    const char* const crew[] = {"du-chief", "du-pilot"};
    for (int k = 0; k < 2; ++k) {
        const std::string cancelled = deployments[k];
        CAPTURE(cancelled);
        // Cancel before the other deployment finishes, and after it (Harvest already waiting).
        for (int after = 0; after < 2; ++after) {
            CAPTURE(after);
            World w;
            w.duneToSplit("RUN-1");
            if (after) w.finish("RUN-1", deployments[1 - k], crew[1 - k], "du-ops");
            w.engine.cancelWork("RUN-1", cancelled, "du-ops");
            if (!after) w.finish("RUN-1", deployments[1 - k], crew[1 - k], "du-ops");

            CHECK_FALSE(w.progress("RUN-1").isWaiting("Harvest"));
            CHECK(w.progress("RUN-1").isCancelled("Harvest"));
            CHECK_FALSE(w.instance("RUN-1")->isStalled());
            CHECK(w.instance("RUN-1")->isComplete());
        }
    }
}

TEST_CASE("ProgressionTracker: dropped steps are reported, cascade, and come back on a retry") {
    // A -> B and C (parallel); D needs B and C; E needs D. Routing: B -> D, C -> D, D -> E.
    DataBasedRouting routing;
    routing.addPath("A", "B"); routing.addPath("A", "C");
    routing.addPath("B", "D"); routing.addPath("C", "D"); routing.addPath("D", "E");
    routing.addPath("C", "Retry"); routing.addPath("Retry", "B");
    DependencyGraph dependencies;
    dependencies.addDependency("D", "B");
    dependencies.addDependency("D", "C");
    dependencies.addDependency("E", "D");
    ProgressionTracker tracker(routing, dependencies, std::vector<std::string>(1, "A"));
    KeyValuePayload data;
    tracker.start();
    tracker.complete("A", data);
    tracker.complete("B", data);
    REQUIRE(tracker.isWaiting("D"));

    // Cancelling B's sibling C: D (already waiting) can never start.
    // (C routes nowhere when cancelled, so this is the cancel path, not routing.)
    ProgressionResult cancelled = tracker.cancel("C");
    CHECK(has(cancelled.dropped, "D"));
    CHECK(tracker.isCancelled("D"));
    CHECK_FALSE(tracker.isWaiting("D"));
    CHECK(tracker.isComplete());
    CHECK(tracker.unmetPrerequisites("D") == std::vector<std::string>(1, "C"));
}

TEST_CASE("ProgressionTracker: a step reached after its prerequisite was cancelled is dropped, not parked") {
    DataBasedRouting routing;
    routing.addPath("A", "B"); routing.addPath("A", "C");
    routing.addPath("B", "D"); routing.addPath("C", "D");
    DependencyGraph dependencies;
    dependencies.addDependency("D", "B");
    dependencies.addDependency("D", "C");
    ProgressionTracker tracker(routing, dependencies, std::vector<std::string>(1, "A"));
    KeyValuePayload data;
    tracker.start();
    tracker.complete("A", data);
    CHECK(tracker.cancel("C").dropped.empty());   // nothing waiting yet
    ProgressionResult done = tracker.complete("B", data);
    CHECK(has(done.dropped, "D"));
    CHECK_FALSE(has(done.waiting, "D"));
    CHECK(tracker.isComplete());
}

TEST_CASE("ProgressionTracker: a dropped step is reached again once its prerequisite is retried") {
    // Retry loop: C is rejected and routed to FixC, which routes back to C.
    DataBasedRouting routing;
    routing.addPath("A", "B"); routing.addPath("A", "C");
    routing.addPath("B", "D");
    routing.addPath("C", "D");
    routing.setRejectionPath("C", std::vector<std::string>(1, "FixC"));
    routing.addPath("FixC", "C");
    DependencyGraph dependencies;
    dependencies.addDependency("D", "B");
    dependencies.addDependency("D", "C");
    ProgressionTracker tracker(routing, dependencies, std::vector<std::string>(1, "A"));
    KeyValuePayload data;
    tracker.start();
    tracker.complete("A", data);
    tracker.complete("B", data);
    REQUIRE(tracker.isWaiting("D"));

    data.set("C.outcome", "rejected");
    ProgressionResult rejected = tracker.reject("C", data);
    CHECK(has(rejected.dropped, "D"));
    CHECK(tracker.isAvailable("FixC"));

    tracker.complete("FixC", data);               // C is reopened
    CHECK(tracker.isAvailable("C"));
    data.set("C.outcome", "completed");
    tracker.complete("C", data);                  // routes C -> D: D is reopened and can start
    CHECK(tracker.isAvailable("D"));
}

}  // TEST_SUITE
