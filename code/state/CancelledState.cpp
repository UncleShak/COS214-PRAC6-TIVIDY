#include "CancelledState.h"
#include "../core/WorkflowException.h"
void CancelledState::makeAvailable(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot makeAvailable"); }
void CancelledState::assign(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot assign"); }
void CancelledState::start(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot start"); }
void CancelledState::complete(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot complete"); }
void CancelledState::reject(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot reject"); }
void CancelledState::cancel(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot cancel"); }
void CancelledState::escalate(WorkItem*) { throw InvalidTransition("Cancelled is terminal: cannot escalate"); }
