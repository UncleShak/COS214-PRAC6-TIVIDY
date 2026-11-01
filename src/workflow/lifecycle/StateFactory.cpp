#include "StateFactory.h"
#include <stdexcept>
#include "AssignedState.h"
#include "AvailableState.h"
#include "CancelledState.h"
#include "CompletedState.h"
#include "CreatedState.h"
#include "EscalatedState.h"
#include "RejectedState.h"
#include "StartedState.h"

WorkItemState* createWorkItemState(const std::string& name) {
    if (name == "Created") return new CreatedState();
    if (name == "Available") return new AvailableState();
    if (name == "Assigned") return new AssignedState();
    if (name == "Started") return new StartedState();
    if (name == "Completed") return new CompletedState();
    if (name == "Rejected") return new RejectedState();
    if (name == "Cancelled") return new CancelledState();
    if (name == "Escalated") return new EscalatedState();
    throw std::invalid_argument("Unknown work item state: " + name);
}
