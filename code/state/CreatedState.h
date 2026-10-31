#ifndef CREATED_STATE_H
#define CREATED_STATE_H

#include "WorkItemState.h"

class CreatedState : public WorkItemState {
public:
    std::string getStateName() const override {
        return "Created";
    }

    void makeAvailable(WorkItem* context) override;
    void assign(WorkItem* context) override;
    void start(WorkItem* context) override;
    void complete(WorkItem* context) override;
    void reject(WorkItem* context) override;
    void cancel(WorkItem* context) override;
    void escalate(WorkItem* context) override;
};

#endif