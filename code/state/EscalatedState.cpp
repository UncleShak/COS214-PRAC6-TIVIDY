#include "EscalatedState.h"
#include "AssignedState.h"
#include "CancelledState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Legal Transitions ---
void EscalatedState::assign(WorkItem* context) {
    // Pushed to a new participant (like a supervisor)[cite: 14].
    context->changeState(new AssignedState());
}

void EscalatedState::cancel(WorkItem* context) {
    // The supervisor decides to abandon the work.
    context->changeState(new CancelledState());
}

// --- Illegal Transitions ---
void EscalatedState::makeAvailable(WorkItem* context) {
    throw std::logic_error("Escalated items must be directly assigned, not made generally available.");
}

void EscalatedState::start(WorkItem* context) {
    throw std::logic_error("Cannot start an Escalated item. It must be reassigned first.");
}

void EscalatedState::complete(WorkItem* context) {
    throw std::logic_error("Cannot complete an Escalated item.");
}

void EscalatedState::reject(WorkItem* context) {
    throw std::logic_error("Cannot reject an Escalated item.");
}

void EscalatedState::escalate(WorkItem* context) {
    throw std::logic_error("Item is already Escalated.");
}