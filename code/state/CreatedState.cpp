#include "CreatedState.h"
#include "AvailableState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Legal Transitions ---
void CreatedState::makeAvailable(WorkItem* context) {
    // Dependencies are met; nobody holds it yet.
    context->changeState(new AvailableState());
}

// --- Illegal Transitions ---
void CreatedState::assign(WorkItem* context) {
    throw std::logic_error("Cannot assign a Created item. Dependencies must be met first.");
}

void CreatedState::start(WorkItem* context) {
    throw std::logic_error("Cannot start a Created item.");
}

void CreatedState::complete(WorkItem* context) {
    throw std::logic_error("Cannot complete a Created item.");
}

void CreatedState::reject(WorkItem* context) {
    throw std::logic_error("Cannot reject a Created item.");
}

void CreatedState::cancel(WorkItem* context) {
    throw std::logic_error("Cannot cancel a Created item. Use the instance to remove it instead.");
}

void CreatedState::escalate(WorkItem* context) {
    throw std::logic_error("Cannot escalate a Created item.");
}