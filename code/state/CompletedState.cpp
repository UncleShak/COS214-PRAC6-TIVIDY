#include "CompletedState.h"
#include "WorkItem.h"
#include <stdexcept>

// --- Final State: All Transitions are Illegal ---
// Once here, nothing more happens to that work item[cite: 14].

void CompletedState::makeAvailable(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot make available.");
}

void CompletedState::assign(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot reassign.");
}

void CompletedState::start(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot restart.");
}

void CompletedState::complete(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed.");
}

void CompletedState::reject(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot reject.");
}

void CompletedState::cancel(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot cancel.");
}

void CompletedState::escalate(WorkItem* context) {
    throw std::logic_error("WorkItem is already Completed. Cannot escalate.");
}