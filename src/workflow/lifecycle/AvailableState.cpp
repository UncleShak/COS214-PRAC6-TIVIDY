// AvailableState.cpp
#include "AvailableState.h"
#include "AssignedState.h"
#include "CancelledState.h"
#include "EscalatedState.h"
#include "WorkItem.h"
#include <stdexcept>
#include <iostream>

// --- Illegal Transitions ---
// If the engine or a participant tries to execute these while the task 
// is only "Available", the state strictly refuses the action

void AvailableState::makeAvailable(WorkItem* context) {
    throw std::logic_error("Transition failed: WorkItem '" + context->getName() + "' is already Available.");
}

void AvailableState::start(WorkItem* context) {
    throw std::logic_error("Transition failed: Cannot start WorkItem '" + context->getName() + "'. It must be Assigned first.");
}

void AvailableState::complete(WorkItem* context) {
    throw std::logic_error("Transition failed: Cannot complete WorkItem '" + context->getName() + "' before it is Started.");
}

void AvailableState::reject(WorkItem* context) {
    throw std::logic_error("Transition failed: Cannot reject WorkItem '" + context->getName() + "' before it is Started.");
}

// --- Legal Transitions ---
// These operations are permitted from the Available state.

void AvailableState::assign(WorkItem* context) {
    // A participant has claimed or been given the work item.
    // Transition to AssignedState.
    context->changeState(new AssignedState());
}

void AvailableState::cancel(WorkItem* context) {
    // The workflow was abandoned before this task was finished.
    // Transition to CancelledState.
    context->changeState(new CancelledState());
}

void AvailableState::escalate(WorkItem* context) {
    // Nobody acted on the available item in time, pushing it to an escalation path.
    // Transition to EscalatedState.
    context->changeState(new EscalatedState());
}