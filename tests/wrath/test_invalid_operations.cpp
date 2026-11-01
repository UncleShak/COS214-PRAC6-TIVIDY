#include "doctest.h"

#include "WorkItem.h"
#include "WorkItemState.h"

#include <stdexcept>
#include <string>

namespace {

typedef void (WorkItem::*WorkItemOp)();

struct OpSpec {
    const char* opName;
    WorkItemOp op;
};

const OpSpec ALL_OPS[] = {
    {"makeAvailable", &WorkItem::makeAvailable},
    {"assign",        &WorkItem::assign},
    {"start",         &WorkItem::start},
    {"complete",      &WorkItem::complete},
    {"reject",        &WorkItem::reject},
    {"cancel",        &WorkItem::cancel},
    {"escalate",      &WorkItem::escalate},
};
const int N_OPS = 7;

void driveToCreated(WorkItem&) {
    // WorkItem starts in CreatedState; no setup needed.
}

void driveToAvailable(WorkItem& item) {
    item.makeAvailable();
}

void driveToAssigned(WorkItem& item) {
    item.makeAvailable();
    item.assign();
}

void driveToStarted(WorkItem& item) {
    item.makeAvailable();
    item.assign();
    item.start();
}

void driveToEscalated(WorkItem& item) {
    item.makeAvailable();
    item.escalate();
}

void driveToCompleted(WorkItem& item) {
    item.makeAvailable();
    item.assign();
    item.start();
    item.complete();
}

void driveToCancelled(WorkItem& item) {
    item.makeAvailable();
    item.cancel();
}

void driveToRejected(WorkItem& item) {
    item.makeAvailable();
    item.assign();
    item.start();
    item.reject();
}

struct StateSpec {
    const char* stateName;
    void (*setup)(WorkItem&);
    const char* next[7];   // nullptr means the op must throw and leave state unchanged
};

// Order matches ALL_OPS: makeAvailable, assign, start, complete, reject, cancel, escalate
const StateSpec ALL_STATES[] = {
    {"Created",   driveToCreated,
     {  "Available", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }},
    {"Available", driveToAvailable,
     {  nullptr, "Assigned", nullptr, nullptr, nullptr, "Cancelled", "Escalated" }},
    // Bug 4: overdue work that is already Assigned or Started can be escalated too.
    {"Assigned",  driveToAssigned,
     {  nullptr, nullptr, "Started", nullptr, nullptr, "Cancelled", "Escalated" }},
    {"Started",   driveToStarted,
     {  nullptr, nullptr, nullptr, "Completed", "Rejected", "Cancelled", "Escalated" }},
    {"Escalated", driveToEscalated,
     {  nullptr, "Assigned", nullptr, nullptr, nullptr, "Cancelled", nullptr }},
    {"Completed", driveToCompleted,
     {  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }},
    {"Cancelled", driveToCancelled,
     {  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }},
    {"Rejected",  driveToRejected,
     {  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }},
};
const int N_STATES = 8;

}  // namespace

TEST_SUITE("wrath") {

TEST_CASE("every legal transition reaches its expected next state") {
    for (int s = 0; s < N_STATES; ++s) {
        for (int i = 0; i < N_OPS; ++i) {
            if (ALL_STATES[s].next[i] == nullptr) continue;

            WorkItem item("W", "role");
            ALL_STATES[s].setup(item);
            REQUIRE(item.getCurrentState()->getStateName() == ALL_STATES[s].stateName);

            CHECK_NOTHROW((item.*ALL_OPS[i].op)());
            CHECK(item.getCurrentState()->getStateName() == std::string(ALL_STATES[s].next[i]));
        }
    }
}

TEST_CASE("every illegal operation throws and leaves the state unchanged") {
    for (int s = 0; s < N_STATES; ++s) {
        for (int i = 0; i < N_OPS; ++i) {
            if (ALL_STATES[s].next[i] != nullptr) continue;

            WorkItem item("W", "role");
            ALL_STATES[s].setup(item);
            REQUIRE(item.getCurrentState()->getStateName() == ALL_STATES[s].stateName);

            CHECK_THROWS_AS((item.*ALL_OPS[i].op)(), std::logic_error);
            CHECK(item.getCurrentState()->getStateName() == ALL_STATES[s].stateName);
        }
    }
}

}  // TEST_SUITE