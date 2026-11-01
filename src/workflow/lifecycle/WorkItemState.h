#ifndef WORK_ITEM_STATE_H
#define WORK_ITEM_STATE_H

#include <string>

class WorkItem;

class WorkItemState{
    public:
        virtual ~WorkItemState() = default;

        // returns the generic name of the state(available, started, etc)
        virtual std::string getStateName() const = 0;

        // The context - WorkItem - delegates to the following methods
        virtual void makeAvailable(WorkItem* context) = 0;
        virtual void assign(WorkItem* context) = 0;
        virtual void start(WorkItem* context) = 0;
        virtual void complete(WorkItem* context) = 0;
        virtual void reject(WorkItem* context) = 0;
        virtual void cancel(WorkItem* context) = 0;
        virtual void escalate(WorkItem* context) = 0;
};

#endif