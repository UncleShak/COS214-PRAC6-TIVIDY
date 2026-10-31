#include "CancelledState.h"
#include "WorkItem.h"
#include "WorkflowException.h"

// --- Final State: All Transitions are Illegal ---
// A cancelled work item is withdrawn for good.

void CancelledState::makeAvailable(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot make available.");
}

void CancelledState::assign(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot assign.");
}

void CancelledState::start(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot start.");
}

void CancelledState::complete(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot complete.");
}

void CancelledState::reject(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot reject.");
}

void CancelledState::cancel(WorkItem*) {
    throw InvalidTransition("WorkItem is already Cancelled.");
}

void CancelledState::escalate(WorkItem*) {
    throw InvalidTransition("WorkItem is Cancelled. Cannot escalate.");
}
