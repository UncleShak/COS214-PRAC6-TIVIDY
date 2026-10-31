#include "Commands.h"

#include "../participants/Participant.h"
#include "../workflow/WorkItem.h"

#include <stdexcept>

namespace {
std::string quoted(const WorkItem* item) { return "'" + item->getName() + "'"; }
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

RejectCommand::RejectCommand(WorkItem* i, Participant* a, const std::string& reason, WorkflowComponent* t)
    : Command(i, a, t), reason(reason) {}

void RejectCommand::execute() {
    target->authorize("reject");
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
    item->cancel();
    if (item->getAssignedParticipant() != nullptr) {
        item->getAssignedParticipant()->workFinished();
    }
}

std::string CancelCommand::describe() const {
    return actor->getName() + " cancelled " + quoted(item);
}

EscalateCommand::EscalateCommand(WorkItem* i, Participant* a, WorkflowComponent* t) : Command(i, a, t) {}

void EscalateCommand::execute() {
    target->authorize("escalate");
    item->escalate();
}

std::string EscalateCommand::describe() const {
    return actor->getName() + " escalated " + quoted(item);
}
