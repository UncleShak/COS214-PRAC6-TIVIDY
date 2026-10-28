#include "Command.h"

#include <stdexcept>

Command::Command(WorkItem* i, Participant* a) : item(i), actor(a) {
    if (item == nullptr || actor == nullptr) {
        throw std::invalid_argument("A command needs a work item and an actor");
    }
}
