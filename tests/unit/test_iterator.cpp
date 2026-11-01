/**
 * @file test_iterator.cpp
 * @brief Envy: Iterator pattern. AvailableWorkIterator from ProgressionTracker::createIterator(),
 *        HistoryIterator over the team's CommandLog and AuditDecorator history.
 */
#include "doctest.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "AuditDecorator.h"
#include "AvailableWorkIterator.h"
#include "CommandLog.h"
#include "Commands.h"
#include "DataBasedRouting.h"
#include "DependencyGraph.h"
#include "History.h"
#include "HistoryIterator.h"
#include "KeyValuePayload.h"
#include "Participant.h"
#include "ProgressionTracker.h"
#include "WorkItem.h"

namespace {
std::vector<std::string> list(const char* a) { return std::vector<std::string>(1, a); }
std::vector<std::string> list(const char* a, const char* b) {
    std::vector<std::string> v; v.push_back(a); v.push_back(b); return v;
}
std::vector<std::string> list(const char* a, const char* b, const char* c) {
    std::vector<std::string> v = list(a, b); v.push_back(c); return v;
}

/// Client code: uses only the Iterator interface, whatever the concrete iterator is.
std::vector<std::string> collect(WorkflowIterator<std::string>& it) {
    std::vector<std::string> out;
    for (it.first(); !it.isDone(); it.next()) out.push_back(it.currentItem());
    return out;
}

class ListHistory : public History {
public:
    std::vector<std::string> entries;
    void record(const std::string& entry) override { entries.push_back(entry); }
};

/// ArkhamExpress: ReceiveOrder splits into PreparePackage + AssignDriver; Dispatch needs both.
DataBasedRouting orderRouting() {
    DataBasedRouting routing;
    routing.addPath("ReceiveOrder", "PreparePackage");
    routing.addPath("ReceiveOrder", "AssignDriver");
    routing.addPath("PreparePackage", "Dispatch");
    routing.addPath("AssignDriver", "Dispatch");
    return routing;
}
}

TEST_SUITE("iterator") {

TEST_CASE("the tracker gives out an iterator over the work available now, in order") {
    DataBasedRouting routing = orderRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, list("ReceiveOrder"));
    KeyValuePayload data;
    tracker.start();
    tracker.complete("ReceiveOrder", data);

    std::unique_ptr<WorkflowIterator<std::string> > it = tracker.createIterator();
    CHECK(collect(*it) == list("PreparePackage", "AssignDriver"));
}

TEST_CASE("completing work while iterating is safe; a new iterator sees the new list") {
    DataBasedRouting routing = orderRouting();
    DependencyGraph deps;
    deps.addDependency("Dispatch", "PreparePackage");
    deps.addDependency("Dispatch", "AssignDriver");
    ProgressionTracker tracker(routing, deps, list("ReceiveOrder"));
    KeyValuePayload data;
    tracker.start();
    tracker.complete("ReceiveOrder", data);

    std::unique_ptr<WorkflowIterator<std::string> > it = tracker.createIterator();
    std::vector<std::string> done;
    for (it->first(); !it->isDone(); it->next()) {
        tracker.complete(it->currentItem(), data);   // changes the tracker's list mid-loop
        done.push_back(it->currentItem());
    }
    CHECK(done == list("PreparePackage", "AssignDriver"));
    CHECK(collect(*tracker.createIterator()) == list("Dispatch"));
}

TEST_CASE("first() restarts, next() past the end is harmless, currentItem() when done throws") {
    AvailableWorkIterator it(list("A", "B"));
    it.first();
    CHECK(it.currentItem() == "A");
    it.next();
    it.next();
    CHECK(it.isDone());
    it.next();                                           // still done, no crash
    CHECK(it.isDone());
    CHECK_THROWS_AS(it.currentItem(), std::out_of_range);
    it.first();
    CHECK(it.currentItem() == "A");
}

TEST_CASE("nothing available means the iterator is done straight away") {
    DataBasedRouting routing = orderRouting();
    DependencyGraph deps;
    ProgressionTracker tracker(routing, deps, list("ReceiveOrder"));   // not started yet
    std::unique_ptr<WorkflowIterator<std::string> > it = tracker.createIterator();
    it->first();
    CHECK(it->isDone());
}

TEST_CASE("history can be read oldest first, newest first, or for one work item") {
    std::vector<std::string> history = list("started: ReceiveOrder", "completed: ReceiveOrder",
                                            "started: Dispatch");
    HistoryIterator oldest(history);
    CHECK(collect(oldest) == history);

    HistoryIterator newest(history, HistoryIterator::NewestFirst);
    CHECK(collect(newest) == list("started: Dispatch", "completed: ReceiveOrder", "started: ReceiveOrder"));

    HistoryIterator oneItem(history, HistoryIterator::OldestFirst, "ReceiveOrder");
    CHECK(oneItem.size() == 2);
    CHECK(collect(oneItem) == list("started: ReceiveOrder", "completed: ReceiveOrder"));

    HistoryIterator none(history, HistoryIterator::OldestFirst, "Refund");
    none.first();
    CHECK(none.isDone());
}

TEST_CASE("history iterator walks the Command log (what each participant did)") {
    WorkItem verify("VerifyDocuments", "Clerk");
    verify.makeAvailable();
    Participant supervisor("Sipho", "P00");
    Participant thandi("Thandi", "P01");

    CommandLog log;
    log.run(new AssignCommand(&verify, &supervisor, &thandi));
    log.run(new StartCommand(&verify, &thandi));
    log.run(new CompleteCommand(&verify, &thandi));

    HistoryIterator it(log.describeAll(), HistoryIterator::NewestFirst);
    CHECK(collect(it) == list("Thandi completed 'VerifyDocuments'",
                              "Thandi started 'VerifyDocuments'",
                              "Sipho assigned 'VerifyDocuments' to Thandi"));
}

TEST_CASE("history iterator walks the entries an AuditDecorator records") {
    WorkItem pack("PreparePackage", "Warehouse");
    pack.makeAvailable();
    pack.assign();
    pack.start();
    ListHistory history;
    AuditDecorator audited(&pack, &history);
    audited.request();                                   // completes the item, audited

    HistoryIterator it(history.entries);
    CHECK(collect(it) == list("started: PreparePackage", "completed: PreparePackage"));
    CHECK(pack.getCurrentState()->getStateName() == "Completed");
}

TEST_CASE("one loop works for every concrete iterator (polymorphism, no type checks)") {
    AvailableWorkIterator available(list("Dispatch"));
    HistoryIterator history(list("started: Dispatch"));
    std::vector<WorkflowIterator<std::string>*> all;
    all.push_back(&available);
    all.push_back(&history);
    std::size_t total = 0;
    for (std::size_t i = 0; i < all.size(); ++i) total += collect(*all[i]).size();
    CHECK(total == 2);
}

}
