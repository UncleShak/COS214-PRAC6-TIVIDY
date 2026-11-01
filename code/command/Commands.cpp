#include "Commands.h"

#include "../participants/Participant.h"
#include "../state/WorkItemState.h"
#include "../workflow/WorkItem.h"

#include <stdexcept>

namespace {
std::string quoted(const WorkItem* item) { return "'" + item->getName() + "'"; }

// True while the assignee is actually holding the work (it counts in their workload).
// After an escalation the old assignee is still recorded but no longer holds it.
bool assigneeHoldsWork(const WorkItem* item) {
    const std::string state = item->getCurrentState()->getStateName();
    return item->getAssignedParticipant() != nullptr && (state == "Assigned" || state == "Started");
}
}  // namespace

AssignCommand::AssignCommand(WorkItem* i, Participant* a, Participant* assignee, WorkflowComponent* t)
    : Command(i, a, t), assignee(assignee) {
    if (assignee == nullptr) {
        throw std::invalid_argument("AssignCommand needs an assignee");
    }
}

void AssignCommand::execute() {
    target->authorize("assign");
    // Change state first: if the state refuses, nobody is recorded or counted.
    item->assign();
    item->assignTo(assignee);
    assignee->workAssigned();
}

std::string AssignCommand::describe() const {
    return actor->getName() + " assigned " + quoted(item) + " to " + assignee->getName();
}

StartCommand::StartCommand(WorkItem* i, Participant* a, WorkflowComponent* t) : Command(i, a, t) {}

void StartCommand::execute() {
    target->authorize("start");
    requireAssignee();
    item->start();
}

std::string StartCommand::describe() const { return actor->getName() + " started " + quoted(item); }

CompleteCommand::CompleteCommand(WorkItem* i, Participant* a, WorkflowComponent* t)
    : Command(i, a, t) {}

void CompleteCommand::execute() {
    target->authorize("complete");
    requireAssignee();
    // Runs the decorators (and a proxy's own check), then WorkItem::request() completes the item.
    target->request();
    actor->workFinished();
}

std::string CompleteCommand::describe() const {
    return actor->getName() + " completed " + quoted(item);
}

RejectCommand::RejectCommand(WorkItem* i, Participant* a, const std::string& reason, WorkflowComponent* t,
                             bool byApprovalChain)
    : Command(i, a, t), reason(reason), byApprovalChain(byApprovalChain) {}

void RejectCommand::execute() {
    target->authorize("reject");
    // Bug 7: holding the right role is not enough; you must be the one doing the work.
    if (!byApprovalChain) requireAssignee();
    item->reject();
    if (item->getAssignedParticipant() != nullptr) {
        item->getAssignedParticipant()->workFinished();
    }
}

std::string RejectCommand::describe() const {
    return actor->getName() + " rejected " + quoted(item) + ": " + reason;
}

CancelCommand::CancelCommand(WorkItem* i, Participant* a, WorkflowComponent* t) : Command(i, a, t) {}

void CancelCommand::execute() {
    target->authorize("cancel");
    const bool held = assigneeHoldsWork(item);   // check before the state changes
    item->cancel();
    if (held) {
        item->getAssignedParticipant()->workFinished();
    }
}

std::string CancelCommand::describe() const {
    return actor->getName() + " cancelled " + quoted(item);
}

EscalateCommand::EscalateCommand(WorkItem* i, Participant* a, WorkflowComponent* t) : Command(i, a, t) {}

void EscalateCommand::execute() {
    target->authorize("escalate");
    const bool held = assigneeHoldsWork(item);   // overdue Assigned or Started work (bug 4)
    item->escalate();
    if (held) {
        item->getAssignedParticipant()->workFinished();   // it is taken off them for reassignment
    }
}

std::string EscalateCommand::describe() const {
    return actor->getName() + " escalated " + quoted(item);
}
