#ifndef AVAILABLE_STATE_H
#define AVAILABLE_STATE_H

#include "WorkItemState.h"

class AvailableState : public WorkItemState {
public:
    std::string getStateName() const override {
        return "Available";
    }

    // Illegal transitions from 'Available' (Engine handles invalid moves)
    void makeAvailable(WorkItem* context) override; 
    void start(WorkItem* context) override;
    void complete(WorkItem* context) override;
    void reject(WorkItem* context) override;

    // Legal transitions from 'Available'
    void assign(WorkItem* context) override;  // Moves to AssignedState
    void cancel(WorkItem* context) override;  // Moves to CancelledState
    void escalate(WorkItem* context) override; // Moves to EscalatedState
};

#endif