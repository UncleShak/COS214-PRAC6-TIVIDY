#include "Commands.h"

#include "../participants/Participant.h"
#include "../workflow/WorkItem.h"

#include <stdexcept>

namespace {
std::string quoted(const WorkItem* item) { return "'" + item->getName() + "'"; }
}  // namespace

AssignCommand::AssignCommand(WorkItem* i, Participant* a, Participant* assignee)
    : Command(i, a), assignee(assignee) {
    if (assignee == nullptr) {
        throw std::invalid_argument("AssignCommand needs an assignee");
    }
}

void AssignCommand::execute() {
    // Change state first: if the state refuses, nobody is recorded or counted.
    item->assign();
    item->assignTo(assignee);
    assignee->workAssigned();
}

std::string AssignCommand::describe() const {
    return actor->getName() + " assigned " + quoted(item) + " to " + assignee->getName();
}

StartCommand::StartCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void StartCommand::execute() {
    requireAssignee();
    item->start();
}

std::string StartCommand::describe() const { return actor->getName() + " started " + quoted(item); }

CompleteCommand::CompleteCommand(WorkItem* i, Participant* a, WorkflowComponent* t)
    : Command(i, a, t) {}

void CompleteCommand::execute() {
    requireAssignee();
    target->request();  // Runs the decorators, then WorkItem::request() completes the item.
    actor->workFinished();
}

std::string CompleteCommand::describe() const {
    return actor->getName() + " completed " + quoted(item);
}

RejectCommand::RejectCommand(WorkItem* i, Participant* a, const std::string& reason)
    : Command(i, a), reason(reason) {}

void RejectCommand::execute() {
    item->reject();
    if (item->getAssignedParticipant() != nullptr) {
        item->getAssignedParticipant()->workFinished();
    }
}

std::string RejectCommand::describe() const {
    return actor->getName() + " rejected " + quoted(item) + ": " + reason;
}

CancelCommand::CancelCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void CancelCommand::execute() {
    item->cancel();
    if (item->getAssignedParticipant() != nullptr) {
        item->getAssignedParticipant()->workFinished();
    }
}

std::string CancelCommand::describe() const {
    return actor->getName() + " cancelled " + quoted(item);
}

EscalateCommand::EscalateCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void EscalateCommand::execute() { item->escalate(); }

std::string EscalateCommand::describe() const {
    return actor->getName() + " escalated " + quoted(item);
}
