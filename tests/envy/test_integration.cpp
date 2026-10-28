/**
 * @file test_integration.cpp
 * @brief Envy's code against the team's types on integration-Code:
 *        WorkflowComponent / WorkItem (by pointer), the abstract DataPayload, and the
 *        WorkItemState outcomes Completed, Rejected and Cancelled.
 *
 * WorkItem.cpp does not compile on integration-Code yet, so these tests use a minimal
 * WorkflowComponent leaf. WorkItem is a WorkflowComponent, so the same calls take a WorkItem*.
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
#include "WorkflowComponent.h"

namespace {
std::vector<std::string> names(const char* a) { return std::vector<std::string>(1, a); }
std::vector<std::string> names(const char* a, const char* b) {
    std::vector<std::string> v; v.push_back(a); v.push_back(b); return v;
}

/// Stand-in for the team's WorkItem: a named WorkflowComponent leaf.
class NamedItem : public WorkflowComponent {
public:
    explicit NamedItem(const std::string& n) : itemName(n) {}
    std::string getName() const override { return itemName; }
    void add(WorkflowComponent*) override { throw std::logic_error("leaf"); }
    void remove(WorkflowComponent*) override { throw std::logic_error("leaf"); }
    void accept(WorkflowVisitor*) override {}
private:
    std::string itemName;
};

/// ArkhamExpress approval: Review -> [approved == yes] Ship / [otherwise] Refund. Ship needs Pack.
DataBasedRouting approvalRouting() {
    DataBasedRouting routing;
    routing.addPath("Receive", "Review");
    routing.addPath("Receive", "Pack");
    routing.addBranch("Review", RoutingCondition::equals("approved", "yes"), names("Ship"));
    routing.setDefaultBranch("Review", names("Refund"));
    return routing;
}
}

TEST_SUITE("integration") {

TEST_CASE("routing takes a WorkItem-style pointer and a DataPayload pointer") {
    DataBasedRouting routing = approvalRouting();
    NamedItem review("Review");
    KeyValuePayload data;
    data.set("approved", "yes");
    DataPayload* payload = &data;   // how WorkflowInstance holds it

    CHECK(routing.determineNextWork(&review, payload).nextItems == names("Ship"));
}

TEST_CASE("a null payload is treated as no data (WorkflowInstance allows a null initial payload)") {
    DataBasedRouting routing = approvalRouting();
    NamedItem review("Review");
    CHECK(routing.determineNextWork(&review, nullptr).nextItems == names("Refund"));
}

TEST_CASE("null work items are refused") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Receive"));
    tracker.start();
    KeyValuePayload data;
    const WorkflowComponent* none = nullptr;
    CHECK_THROWS_AS(routing.determineNextWork(none, &data), std::invalid_argument);
    CHECK_THROWS_AS(tracker.complete(none, &data), std::invalid_argument);
    CHECK_THROWS_AS(tracker.cancel(none), std::invalid_argument);
}

TEST_CASE("completing through pointers reports the Completed outcome and what to make available") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Receive"));
    NamedItem receive("Receive");
    KeyValuePayload data;

    tracker.start();
    ProgressionResult r = tracker.complete(&receive, &data);
    CHECK(r.outcome == "Completed");
    CHECK(r.madeAvailable == names("Review", "Pack"));
    CHECK(r.reopened.empty());
    CHECK(tracker.isCompleted("Receive"));
}

TEST_CASE("rejection path: a rejected item routes to recovery and does not satisfy dependencies") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    deps.addDependency("Ship", "Review");
    ProgressionTracker tracker(routing, deps, names("Receive"));
    NamedItem receive("Receive"), review("Review");
    KeyValuePayload data;
    data.set("approved", "no");

    tracker.start();
    tracker.complete(&receive, &data);
    ProgressionResult r = tracker.reject(&review, &data);

    CHECK(r.outcome == "Rejected");
    CHECK(r.madeAvailable == names("Refund"));
    CHECK(tracker.isRejected("Review"));
    CHECK_FALSE(tracker.isCompleted("Review"));
    CHECK(tracker.unmetPrerequisites("Ship") == names("Review"));
}

TEST_CASE("cancelled work is removed so the instance can still finish") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Receive"));
    KeyValuePayload data;
    data.set("approved", "yes");

    tracker.start();
    tracker.complete("Receive", data);
    tracker.complete("Review", data);
    CHECK_FALSE(tracker.isComplete());     // Pack and Ship are still available

    NamedItem pack("Pack");
    tracker.cancel(&pack);
    CHECK(tracker.isCancelled("Pack"));
    CHECK_FALSE(tracker.isAvailable("Pack"));

    CHECK(tracker.complete("Ship", data).instanceComplete);
}

TEST_CASE("only Available or waiting work can be cancelled") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Receive"));
    KeyValuePayload data;
    CHECK_THROWS_AS(tracker.cancel("Receive"), std::logic_error);   // not started
    tracker.start();
    tracker.complete("Receive", data);
    CHECK_THROWS_AS(tracker.cancel("Receive"), std::logic_error);   // already Completed
    CHECK_THROWS_AS(tracker.cancel("Nowhere"), std::logic_error);   // never reached
}

TEST_CASE("finished work cannot be finished again (matches the final Completed/Rejected states)") {
    DataBasedRouting routing = approvalRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Receive"));
    KeyValuePayload data;
    tracker.start();
    tracker.reject("Receive", data);
    CHECK_THROWS_AS(tracker.complete("Receive", data), std::logic_error);
    CHECK_THROWS_AS(tracker.reject("Receive", data), std::logic_error);
}

TEST_CASE("a retry loop reports reopened items, since the old WorkItem is in a final state") {
    DataBasedRouting routing;
    routing.addPath("Dispatch", "Deliver");
    routing.addBranch("Deliver", RoutingCondition::equals("delivered", "no"), names("Dispatch"));
    routing.setDefaultBranch("Deliver", names("CloseOrder"));
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, names("Dispatch"));
    KeyValuePayload failed;
    failed.set("delivered", "no");

    tracker.start();
    tracker.complete("Dispatch", failed);
    ProgressionResult r = tracker.reject("Deliver", failed);
    CHECK(r.madeAvailable == names("Dispatch"));
    CHECK(r.reopened == names("Dispatch"));
    CHECK(tracker.isAvailable("Dispatch"));
    CHECK_FALSE(tracker.isCompleted("Dispatch"));
}

}
