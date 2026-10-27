#include "StartedState.h"
#include "CompletedState.h"
#include "RejectedState.h"
#include "CancelledState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Illegal Transitions ---
void StartedState::makeAvailable(WorkItem* context) {
    throw std::logic_error("Cannot make a Started item Available.");
}

void StartedState::assign(WorkItem* context) {
    throw std::logic_error("Cannot assign an item that is already Started.");
}

void StartedState::start(WorkItem* context) {
    throw std::logic_error("Item is already Started.");
}

void StartedState::escalate(WorkItem* context) {
    throw std::logic_error("Cannot escalate an actively Started item.");
}

// --- Legal Transitions ---
void StartedState::complete(WorkItem* context) {
    // Work finished successfully[cite: 14].
    context->changeState(new CompletedState());
}

void StartedState::reject(WorkItem* context) {
    // Work finished, but the outcome was 'no'[cite: 14].
    context->changeState(new RejectedState());
}

void StartedState::cancel(WorkItem* context) {
    // Abandoned before finishing[cite: 14].
    context->changeState(new CancelledState());
}