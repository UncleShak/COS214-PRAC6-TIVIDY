/**
 * @file StandInState.h
 * @brief Test-only WorkItemState with a settable name (see support/README.md).
 *
 * Every trigger moves straight to the matching state name with no legality checks.
 * The real rules live in Pride's concrete states; these tests are about Visitor and
 * Iterator, not about the lifecycle.
 */
#ifndef TIVIDY_TEST_STAND_IN_STATE_H
#define TIVIDY_TEST_STAND_IN_STATE_H

#include <string>

#include "WorkItem.h"
#include "WorkItemState.h"

class StandInState : public WorkItemState {
public:
    explicit StandInState(const std::string& stateName) : name(stateName) {}
    std::string getStateName() const override { return name; }

    void makeAvailable(WorkItem* c) override { c->changeState(new StandInState("Available")); }
    void assign(WorkItem* c) override { c->changeState(new StandInState("Assigned")); }
    void start(WorkItem* c) override { c->changeState(new StandInState("Started")); }
    void complete(WorkItem* c) override { c->changeState(new StandInState("Completed")); }
    void reject(WorkItem* c) override { c->changeState(new StandInState("Rejected")); }
    void cancel(WorkItem* c) override { c->changeState(new StandInState("Cancelled")); }
    void escalate(WorkItem* c) override { c->changeState(new StandInState("Escalated")); }

private:
    std::string name;
};

#endif
