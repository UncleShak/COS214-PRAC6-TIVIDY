#include "RejectedState.h"
#include "../core/WorkflowException.h"
void RejectedState::makeAvailable(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot makeAvailable"); }
void RejectedState::assign(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot assign"); }
void RejectedState::start(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot start"); }
void RejectedState::complete(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot complete"); }
void RejectedState::reject(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot reject"); }
void RejectedState::cancel(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot cancel"); }
void RejectedState::escalate(WorkItem*) { throw InvalidTransition("Rejected is terminal: cannot escalate"); }
