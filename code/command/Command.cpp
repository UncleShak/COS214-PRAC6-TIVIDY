#include "Command.h"

#include "../participants/Participant.h"
#include "../workflow/WorkItem.h"

#include <stdexcept>

Command::Command(WorkItem* i, Participant* a, WorkflowComponent* t)
    : item(i), actor(a), target(t) {
    if (item == nullptr || actor == nullptr) {
        throw std::invalid_argument("A command needs a work item and an actor");
    }
    if (target == nullptr) {
        target = item;  // No decorators: perform the work on the item itself.
    }
}

void Command::requireAssignee() const {
    if (item->getAssignedParticipant() != actor) {
        throw std::logic_error(actor->getName() + " is not assigned to '" + item->getName() + "'");
    }
}
