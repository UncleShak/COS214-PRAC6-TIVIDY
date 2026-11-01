/**
 * @file test_lifecycle.cpp
 * @brief Work-item lifecycle (State pattern): legal transitions advance the state and
 *        illegal ones throw std::logic_error without changing it.
 */
#include "doctest.h"

#include <stdexcept>
#include <string>

#include "WorkItem.h"
#include "WorkItemState.h"

namespace {

std::string stateOf(const WorkItem& item) { return item.getCurrentState()->getStateName(); }

// Drives a fresh item to Started along the happy path.
void startItem(WorkItem& item) {
    item.makeAvailable();
    item.assign();
    item.start();
}

}  // namespace

TEST_SUITE("work item lifecycle") {

TEST_CASE("valid transitions advance the state along the happy path") {
    WorkItem task("Deliver Parcel", "Courier");
    CHECK(stateOf(task) == "Created");

    CHECK_NOTHROW(task.makeAvailable());
    CHECK(stateOf(task) == "Available");

    CHECK_NOTHROW(task.assign());
    CHECK(stateOf(task) == "Assigned");

    CHECK_NOTHROW(task.start());
    CHECK(stateOf(task) == "Started");

    CHECK_NOTHROW(task.complete());
    CHECK(stateOf(task) == "Completed");
}

TEST_CASE("invalid transitions throw logic_error and leave the state unchanged") {
    WorkItem task("Review Application", "Reviewer");

    CHECK_THROWS_AS(task.complete(), std::logic_error);   // cannot complete a Created item
    CHECK_THROWS_AS(task.start(), std::logic_error);      // nor start it before assignment
    CHECK(stateOf(task) == "Created");

    task.makeAvailable();
    CHECK_THROWS_AS(task.complete(), std::logic_error);   // still not Started
    CHECK_THROWS_AS(task.makeAvailable(), std::logic_error);
    CHECK(stateOf(task) == "Available");
}

TEST_CASE("a started item can be rejected, and Rejected is terminal") {
    WorkItem task("Approve Claim", "Assessor");
    startItem(task);

    CHECK_NOTHROW(task.reject());
    CHECK(stateOf(task) == "Rejected");
    CHECK_THROWS_AS(task.complete(), std::logic_error);
    CHECK_THROWS_AS(task.makeAvailable(), std::logic_error);
    CHECK(stateOf(task) == "Rejected");
}

TEST_CASE("an escalated item must be reassigned before work resumes") {
    WorkItem task("Inspect Shipment", "Inspector");
    task.makeAvailable();
    task.assign();

    CHECK_NOTHROW(task.escalate());
    CHECK(stateOf(task) == "Escalated");
    CHECK_THROWS_AS(task.start(), std::logic_error);
    CHECK_THROWS_AS(task.makeAvailable(), std::logic_error);

    CHECK_NOTHROW(task.assign());
    CHECK(stateOf(task) == "Assigned");
    CHECK_NOTHROW(task.start());
    CHECK_NOTHROW(task.complete());
    CHECK(stateOf(task) == "Completed");
}

TEST_CASE("a cancelled item accepts no further actions") {
    WorkItem task("Pack Order", "Packer");
    task.makeAvailable();

    CHECK_NOTHROW(task.cancel());
    CHECK(stateOf(task) == "Cancelled");
    CHECK_THROWS_AS(task.assign(), std::logic_error);
    CHECK_THROWS_AS(task.cancel(), std::logic_error);
    CHECK(stateOf(task) == "Cancelled");
}

TEST_CASE("a completed item rejects every further transition") {
    WorkItem task("Ship Order", "Dispatcher");
    startItem(task);
    task.complete();

    CHECK_THROWS_AS(task.makeAvailable(), std::logic_error);
    CHECK_THROWS_AS(task.assign(), std::logic_error);
    CHECK_THROWS_AS(task.start(), std::logic_error);
    CHECK_THROWS_AS(task.complete(), std::logic_error);
    CHECK_THROWS_AS(task.reject(), std::logic_error);
    CHECK_THROWS_AS(task.cancel(), std::logic_error);
    CHECK_THROWS_AS(task.escalate(), std::logic_error);
    CHECK(stateOf(task) == "Completed");
}

TEST_CASE("reopen returns an item to Created and clears its assignee") {
    WorkItem task("Collect Sample", "Nurse");
    startItem(task);
    task.reject();

    task.reopen();
    CHECK(stateOf(task) == "Created");
    CHECK(task.getAssignedParticipant() == nullptr);
    CHECK_NOTHROW(task.makeAvailable());
}

}  // TEST_SUITE
