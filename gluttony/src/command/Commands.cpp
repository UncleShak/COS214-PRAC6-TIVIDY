#include "Commands.h"

#include "../engine/Participant.h"
#include "../engine/WorkItem.h"

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

void AssignCommand::execute() { item->assign(assignee); }

std::string AssignCommand::describe() const {
    return actor->getName() + " assigned " + quoted(item) + " to " + assignee->getName();
}

StartCommand::StartCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void StartCommand::execute() { item->start(actor); }

std::string StartCommand::describe() const { return actor->getName() + " started " + quoted(item); }

CompleteCommand::CompleteCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void CompleteCommand::execute() { item->complete(actor); }

std::string CompleteCommand::describe() const {
    return actor->getName() + " completed " + quoted(item);
}

RejectCommand::RejectCommand(WorkItem* i, Participant* a, const std::string& reason)
    : Command(i, a), reason(reason) {}

void RejectCommand::execute() { item->reject(actor, reason); }

std::string RejectCommand::describe() const {
    return actor->getName() + " rejected " + quoted(item) + ": " + reason;
}

CancelCommand::CancelCommand(WorkItem* i, Participant* a) : Command(i, a) {}

void CancelCommand::execute() { item->cancel(actor); }

std::string CancelCommand::describe() const {
    return actor->getName() + " cancelled " + quoted(item);
}
