#include "AssignedState.h"
#include "StartedState.h"
#include "CancelledState.h"
#include "AvailableState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Illegal Transitions ---
void AssignedState::makeAvailable(WorkItem* context) {
    // Note: Depending on your team's rules, you might allow this if a participant drops the task.
    // Assuming standard forward-only progression for now.
    throw std::logic_error("Cannot make an Assigned item Available again.");
}

void AssignedState::assign(WorkItem* context) {
    throw std::logic_error("Item is already Assigned.");
}

void AssignedState::complete(WorkItem* context) {
    throw std::logic_error("Cannot complete an item before it is Started.");
}

void AssignedState::reject(WorkItem* context) {
    throw std::logic_error("Cannot reject an item before it is Started.");
}

void AssignedState::escalate(WorkItem* context) {
    throw std::logic_error("Escalation happens from Available state deadlines, not Assigned.");
}

// --- Legal Transitions ---
void AssignedState::start(WorkItem* context) {
    // The participant begins execution.
    context->changeState(new StartedState());
}

void AssignedState::cancel(WorkItem* context) {
    context->changeState(new CancelledState());
}