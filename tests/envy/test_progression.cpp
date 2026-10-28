/**
 * @file test_progression.cpp
 * @brief Envy: progression through a workflow once work completes (ProgressionTracker).
 *
 * Test data: the ArkhamExpress order-delivery process from Practical 6.
 *
 *   ReceiveOrder -> ValidateOrder -> [orderValid == yes] CheckStock / [otherwise] CancelOrder
 *   CheckStock   -> [inStock == yes] PreparePackage / [otherwise] RequestStock -> PreparePackage
 *   PreparePackage -> SelectDeliveryType + AssignDriver   (parallel split)
 *   Dispatch needs PreparePackage, SelectDeliveryType and AssignDriver   (join)
 *   Dispatch -> Deliver -> [delivered == yes] CloseOrder / [otherwise] ReturnToWarehouse
 */
#include "doctest.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "DataBasedRouting.h"
#include "DependencyGraph.h"
#include "KeyValuePayload.h"
#include "ProgressionTracker.h"
#include "RoutingCondition.h"

namespace {
std::vector<std::string> names(const char* a) { return std::vector<std::string>(1, a); }
std::vector<std::string> names(const char* a, const char* b) {
    std::vector<std::string> v; v.push_back(a); v.push_back(b); return v;
}

/// The parts of a WorkflowDefinition Envy owns: shared by every instance.
struct DeliveryDefinition {
    DataBasedRouting routing;
    DependencyGraph dependencies;
    std::vector<std::string> entry;

    DeliveryDefinition() : entry(names("ReceiveOrder")) {
        routing.addPath("ReceiveOrder", "ValidateOrder");
        routing.addBranch("ValidateOrder", RoutingCondition::equals("orderValid", "yes"), names("CheckStock"));
        routing.setDefaultBranch("ValidateOrder", names("CancelOrder"));
        routing.addBranch("CheckStock", RoutingCondition::equals("inStock", "yes"), names("PreparePackage"));
        routing.setDefaultBranch("CheckStock", names("RequestStock"));
        routing.addPath("RequestStock", "PreparePackage");
        routing.addPath("PreparePackage", "SelectDeliveryType");
        routing.addPath("PreparePackage", "AssignDriver");
        routing.addPath("SelectDeliveryType", "Dispatch");
        routing.addPath("AssignDriver", "Dispatch");
        routing.addPath("Dispatch", "Deliver");
        routing.addBranch("Deliver", RoutingCondition::equals("delivered", "yes"), names("CloseOrder"));
        routing.setDefaultBranch("Deliver", names("ReturnToWarehouse"));

        dependencies.addDependency("PreparePackage", "CheckStock");
        dependencies.addDependency("Dispatch", "PreparePackage");
        dependencies.addDependency("Dispatch", "SelectDeliveryType");
        dependencies.addDependency("Dispatch", "AssignDriver");
        dependencies.addDependency("Deliver", "Dispatch");
    }
};

KeyValuePayload validOrderInStock() {
    KeyValuePayload data;
    data.set("orderId", "AX-10482");
    data.set("orderValid", "yes");
    data.set("inStock", "yes");
    data.set("delivered", "yes");
    return data;
}
}

TEST_SUITE("progression") {

TEST_CASE("starting makes the entry work available") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    CHECK_FALSE(tracker.isStarted());
    CHECK(tracker.start() == names("ReceiveOrder"));
    CHECK(tracker.isAvailable("ReceiveOrder"));
    CHECK_FALSE(tracker.isComplete());
    CHECK_THROWS_AS(tracker.start(), std::logic_error);
}

TEST_CASE("completion path: a valid order runs end to end") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    KeyValuePayload data = validOrderInStock();
    tracker.start();

    CHECK(tracker.complete("ReceiveOrder", data).madeAvailable == names("ValidateOrder"));

    ProgressionResult validated = tracker.complete("ValidateOrder", data);
    CHECK(validated.madeAvailable == names("CheckStock"));
    CHECK(validated.decision.description == "Decision at ValidateOrder: [orderValid == yes] -> CheckStock");

    CHECK(tracker.complete("CheckStock", data).madeAvailable == names("PreparePackage"));
    CHECK(tracker.complete("PreparePackage", data).madeAvailable == names("SelectDeliveryType", "AssignDriver"));
    tracker.complete("SelectDeliveryType", data);
    CHECK(tracker.complete("AssignDriver", data).madeAvailable == names("Dispatch"));
    CHECK(tracker.complete("Dispatch", data).madeAvailable == names("Deliver"));
    CHECK(tracker.complete("Deliver", data).madeAvailable == names("CloseOrder"));

    ProgressionResult closed = tracker.complete("CloseOrder", data);
    CHECK(closed.instanceComplete);
    CHECK(tracker.isComplete());
    CHECK_FALSE(tracker.isStalled());
    CHECK(tracker.isCompleted("CloseOrder"));
    CHECK_FALSE(tracker.isCompleted("CancelOrder")); // the branch not taken never ran
}

TEST_CASE("exception path: an invalid order is routed to cancellation") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    KeyValuePayload data;
    data.set("orderValid", "no");
    tracker.start();
    tracker.complete("ReceiveOrder", data);

    ProgressionResult result = tracker.complete("ValidateOrder", data);
    CHECK(result.madeAvailable == names("CancelOrder"));
    CHECK_FALSE(tracker.isAvailable("CheckStock"));

    CHECK(tracker.complete("CancelOrder", data).instanceComplete);
}

TEST_CASE("a missing-stock branch rejoins the main path") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    KeyValuePayload data = validOrderInStock();
    data.set("inStock", "no");
    tracker.start();
    tracker.complete("ReceiveOrder", data);
    tracker.complete("ValidateOrder", data);

    CHECK(tracker.complete("CheckStock", data).madeAvailable == names("RequestStock"));
    CHECK(tracker.complete("RequestStock", data).madeAvailable == names("PreparePackage"));
}

TEST_CASE("dependencies block work until met: a join waits for every parallel branch") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    KeyValuePayload data = validOrderInStock();
    tracker.start();
    tracker.complete("ReceiveOrder", data);
    tracker.complete("ValidateOrder", data);
    tracker.complete("CheckStock", data);
    tracker.complete("PreparePackage", data);

    // First branch reaches Dispatch, but AssignDriver is not done yet.
    ProgressionResult first = tracker.complete("SelectDeliveryType", data);
    CHECK(first.madeAvailable.empty());
    CHECK(first.waiting == names("Dispatch"));
    CHECK(tracker.isWaiting("Dispatch"));
    CHECK(tracker.unmetPrerequisites("Dispatch") == names("AssignDriver"));

    // Waiting work cannot be completed.
    CHECK_THROWS_AS(tracker.complete("Dispatch", data), std::logic_error);

    // The last prerequisite releases it.
    ProgressionResult second = tracker.complete("AssignDriver", data);
    CHECK(second.madeAvailable == names("Dispatch"));
    CHECK(second.waiting.empty());
    CHECK(tracker.isAvailable("Dispatch"));
}

TEST_CASE("invalid operations are refused and leave progress unchanged") {
    DeliveryDefinition def;
    ProgressionTracker tracker(def.routing, def.dependencies, def.entry);
    KeyValuePayload data = validOrderInStock();

    CHECK_THROWS_AS(tracker.complete("ReceiveOrder", data), std::logic_error); // not started
    tracker.start();
    CHECK_THROWS_AS(tracker.complete("Deliver", data), std::logic_error);      // never reached
    tracker.complete("ReceiveOrder", data);
    CHECK_THROWS_AS(tracker.complete("ReceiveOrder", data), std::logic_error); // already complete

    CHECK(tracker.availableItems() == names("ValidateOrder"));
    CHECK(tracker.completedItems().size() == 1);
}

TEST_CASE("a routing error leaves the completed item available to retry") {
    DataBasedRouting routing;
    routing.addBranch("ValidateOrder", RoutingCondition::equals("orderValid", "yes"), names("CheckStock"));
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("ValidateOrder"));
    tracker.start();

    KeyValuePayload noData;
    CHECK_THROWS_AS(tracker.complete("ValidateOrder", noData), std::runtime_error);
    CHECK(tracker.isAvailable("ValidateOrder"));
    CHECK_FALSE(tracker.isCompleted("ValidateOrder"));
}

TEST_CASE("instances of one definition progress independently") {
    DeliveryDefinition def;
    ProgressionTracker first(def.routing, def.dependencies, def.entry);
    ProgressionTracker second(def.routing, def.dependencies, def.entry);
    KeyValuePayload valid = validOrderInStock();
    KeyValuePayload invalid;
    invalid.set("orderValid", "no");

    first.start();
    second.start();
    first.complete("ReceiveOrder", valid);
    first.complete("ValidateOrder", valid);

    // The second instance has not moved.
    CHECK(second.availableItems() == names("ReceiveOrder"));
    CHECK(second.completedItems().empty());

    second.complete("ReceiveOrder", invalid);
    second.complete("ValidateOrder", invalid);

    // Each took its own branch from the same shared definition.
    CHECK(first.isAvailable("CheckStock"));
    CHECK_FALSE(first.isAvailable("CancelOrder"));
    CHECK(second.isAvailable("CancelOrder"));
    CHECK_FALSE(second.isAvailable("CheckStock"));
}

TEST_CASE("routing back to finished work reopens it (retry loop)") {
    DataBasedRouting routing;
    routing.addPath("Dispatch", "Deliver");
    routing.addBranch("Deliver", RoutingCondition::equals("delivered", "no"), names("Dispatch"));
    routing.setDefaultBranch("Deliver", names("CloseOrder"));
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Dispatch"));
    KeyValuePayload failed;
    failed.set("delivered", "no");
    KeyValuePayload success;
    success.set("delivered", "yes");

    tracker.start();
    tracker.complete("Dispatch", failed);
    CHECK(tracker.complete("Deliver", failed).madeAvailable == names("Dispatch"));
    CHECK_FALSE(tracker.isCompleted("Dispatch"));

    tracker.complete("Dispatch", success);
    CHECK(tracker.complete("Deliver", success).madeAvailable == names("CloseOrder"));
}

TEST_CASE("a join that can never be satisfied is reported as stalled") {
    // Misconfigured: Dispatch needs both alternatives of an exclusive decision.
    DataBasedRouting routing;
    routing.addBranch("ChooseCarrier", RoutingCondition::equals("carrier", "own"), names("AssignDriver"));
    routing.setDefaultBranch("ChooseCarrier", names("BookCourier"));
    routing.addPath("AssignDriver", "Dispatch");
    routing.addPath("BookCourier", "Dispatch");
    DependencyGraph deps;
    deps.addDependency("Dispatch", "AssignDriver");
    deps.addDependency("Dispatch", "BookCourier");

    ProgressionTracker tracker(routing, deps, names("ChooseCarrier"));
    KeyValuePayload data;
    data.set("carrier", "own");
    tracker.start();
    tracker.complete("ChooseCarrier", data);
    tracker.complete("AssignDriver", data);

    CHECK(tracker.isStalled());
    CHECK_FALSE(tracker.isComplete());
    CHECK(tracker.unmetPrerequisites("Dispatch") == names("BookCourier"));
}

TEST_CASE("an entry item with unmet prerequisites waits") {
    SequentialRouting routing;
    DependencyGraph deps;
    deps.addDependency("B", "A");
    ProgressionTracker tracker(routing, deps, names("A", "B"));
    CHECK(tracker.start() == names("A"));
    CHECK(tracker.isWaiting("B"));
    CHECK(tracker.complete("A", KeyValuePayload()).madeAvailable == names("B"));
}

TEST_CASE("a workflow needs an entry item") {
    SequentialRouting routing;
    DependencyGraph deps;
    CHECK_THROWS_AS(ProgressionTracker(routing, deps, std::vector<std::string>()), std::invalid_argument);
}

}
