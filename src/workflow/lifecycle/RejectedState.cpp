#include "RejectedState.h"
#include "WorkItem.h"
#include "WorkflowException.h"

// --- Final State: All Transitions are Illegal ---
// The participant refused the work; routing decides what happens next, not this item.

void RejectedState::makeAvailable(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot make available.");
}

void RejectedState::assign(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot assign.");
}

void RejectedState::start(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot start.");
}

void RejectedState::complete(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot complete.");
}

void RejectedState::reject(WorkItem*) {
    throw InvalidTransition("WorkItem is already Rejected.");
}

void RejectedState::cancel(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot cancel.");
}

void RejectedState::escalate(WorkItem*) {
    throw InvalidTransition("WorkItem is Rejected. Cannot escalate.");
}
