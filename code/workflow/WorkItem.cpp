#include "WorkItem.h"
#include "WorkItemState.h"
#include "CreatedState.h" // Default starting state
#include "Event.h"        // For Observer notification

WorkItem::WorkItem(std::string name, std::string role) 
    : itemName(name),requiredRole(role), currentState(new CreatedState()), assignedParticipant(nullptr) {
}

WorkItem::~WorkItem() {
    delete currentState;
}

std::string WorkItem::getName() const {
    return itemName;
}

std::string WorkItem::getRequiredRole() const {
    return requiredRole;
}

void WorkItem::changeState(WorkItemState* newState) {
   if (newState == nullptr) {
        throw std::invalid_argument("New state cannot be null");
    }

    if (currentState == newState) {
        return;
    }

    WorkItemState* oldState = currentState;
    currentState = newState;
    
    // Observer pattern: Notify listeners (like WorkflowInstance) that state changed
    notifyListeners(new Event("STATE_CHANGED", this));

    delete oldState;
}

WorkItemState* WorkItem::getCurrentState() const {
    return currentState;
}

void WorkItem::assignTo(Participant* participant) {
    if (participant == nullptr) {
        throw std::invalid_argument("Participant cannot be null");
    }
    
    assignedParticipant = participant;
}

Participant* WorkItem::getAssignedParticipant() const {
    return assignedParticipant;
}

// Context delegating lifecycle operations to the current state object
void WorkItem::makeAvailable() { currentState->makeAvailable(this); }
void WorkItem::assign() { currentState->assign(this); }
void WorkItem::start() { currentState->start(this); }
void WorkItem::complete() { currentState->complete(this); }
void WorkItem::reject() { currentState->reject(this); }
void WorkItem::cancel() { currentState->cancel(this); }
void WorkItem::escalate() { currentState->escalate(this); }

void WorkItem::add(WorkflowComponent*) {
    throw std::logic_error(
        "A WorkItem cannot contain child components"
    );
}

void WorkItem::remove(WorkflowComponent*) {
    throw std::logic_error(
        "A WorkItem cannot contain child components"
    );
}