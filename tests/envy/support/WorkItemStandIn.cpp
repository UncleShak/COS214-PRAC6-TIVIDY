/**
 * @file WorkItemStandIn.cpp
 * @brief Test-only definition of the team's WorkItem (code/workflow/WorkItem.h).
 *
 * Linked into envy_tests instead of code/workflow/WorkItem.cpp, which cannot compile yet
 * (see support/README.md). Same class and header; the state starts as "Created".
 */
#include "WorkItem.h"

#include <stdexcept>

#include "StandInState.h"
#include "WorkflowVisitor.h"

WorkItem::WorkItem(std::string name, std::string role)
    : itemName(name), requiredRole(role), currentState(new StandInState("Created")),
      assignedParticipant(nullptr) {}

WorkItem::~WorkItem() { delete currentState; }

std::string WorkItem::getName() const { return itemName; }
std::string WorkItem::getRequiredRole() const { return requiredRole; }

void WorkItem::changeState(WorkItemState* newState) {
    if (newState == nullptr) throw std::invalid_argument("New state cannot be null");
    if (newState == currentState) return;
    WorkItemState* old = currentState;
    currentState = newState;
    delete old;
}

WorkItemState* WorkItem::getCurrentState() const { return currentState; }

void WorkItem::assignTo(Participant* participant) {
    if (participant == nullptr) throw std::invalid_argument("Participant cannot be null");
    assignedParticipant = participant;
}

Participant* WorkItem::getAssignedParticipant() const { return assignedParticipant; }

void WorkItem::makeAvailable() { currentState->makeAvailable(this); }
void WorkItem::assign() { currentState->assign(this); }
void WorkItem::start() { currentState->start(this); }
void WorkItem::complete() { currentState->complete(this); }
void WorkItem::reject() { currentState->reject(this); }
void WorkItem::cancel() { currentState->cancel(this); }
void WorkItem::escalate() { currentState->escalate(this); }

void WorkItem::request() { complete(); }

void WorkItem::add(WorkflowComponent*) { throw std::logic_error("A WorkItem cannot contain child components"); }
void WorkItem::remove(WorkflowComponent*) { throw std::logic_error("A WorkItem cannot contain child components"); }

// The one line WorkItem.cpp still needs (Visitor: ConcreteElement).
void WorkItem::accept(WorkflowVisitor* visitor) { visitor->visitWorkItem(this); }
