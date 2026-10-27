#include "WorkItem.h"
#include "WorkItemState.h"
#include "CreatedState.h" // Default starting state
#include "Event.h"        // For Observer notification

WorkItem::WorkItem(std::string name) 
    : itemName(name), currentState(new CreatedState()), assignedParticipant(nullptr) {
}

WorkItem::~WorkItem() {
    delete currentState;
}

std::string WorkItem::getName() const {
    return itemName;
}

void WorkItem::changeState(WorkItemState* newState) {
    if (currentState != nullptr) {
        delete currentState;
    }
    currentState = newState;
    
    // Observer pattern: Notify listeners (like WorkflowInstance) that state changed
    notifyListeners(new Event("STATE_CHANGED", this));
}

WorkItemState* WorkItem::getCurrentState() const {
    return currentState;
}

void WorkItem::assignTo(Participant* participant) {
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