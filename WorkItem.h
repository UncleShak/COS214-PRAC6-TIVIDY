#ifndef WORK_ITEM_H
#define WORK_ITEM_H

#include "WorkflowComponent.h"
#include "WorkItemState.h"
#include "Participant.h"

class WorkItem : public WorkflowComponent{
    public:
        virtual void chnageState(WorkItemState* newState) = 0;
        virtual WorkItemState* getCurrentState() const = 0;
        virtual void assignTo(Participant* participant) = 0;
        virtual Participant* getAssignedParticipant() const = 0;

        // observer pattern
        virtual void notifyListeners() = 0;
};

#endif