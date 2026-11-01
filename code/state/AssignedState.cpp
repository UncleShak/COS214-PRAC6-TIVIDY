#include "AssignedState.h"
#include "StartedState.h"
#include "CancelledState.h"
#include "EscalatedState.h"
#include "AvailableState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Illegal Transitions ---
void AssignedState::makeAvailable(WorkItem* context) {
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


// --- Legal Transitions ---
void AssignedState::start(WorkItem* context) {
    // The participant begins execution.
    context->changeState(new StartedState());
}

void AssignedState::escalate(WorkItem* context) {
    // The deadline passed before the assignee started (bug 4): reassign it.
    context->changeState(new EscalatedState());
}

void AssignedState::cancel(WorkItem* context) {
    context->changeState(new CancelledState());
}