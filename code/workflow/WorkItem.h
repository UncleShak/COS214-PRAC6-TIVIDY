#ifndef WORK_ITEM_H
#define WORK_ITEM_H

// Leaf node and is a single unit of work
#include <string>
#include "WorkflowComponent.h"
#include "WorkItemState.h"
#include "EventSource.h"

class Participant;
class WorkflowVisitor;

// Observer: a WorkItem is the Subject; listeners hear every state change.
class WorkItem : public WorkflowComponent, public EventSource {
    private:
    std::string itemName;
    std::string requiredRole;
    WorkItemState* currentState;
    Participant* assignedParticipant;

    public:
        WorkItem(std::string name, std::string requiredRole );
        virtual ~WorkItem();

        std::string getName() const override;

        // Decorator: ConcreteComponent. Performing the work means completing it, so
        // decorators (validation, deadline, audit, retry) guard completion.
        void request() override;
        std::string getRequiredRole() const;

        void changeState(WorkItemState* newState) ;
        WorkItemState* getCurrentState() const ;

        void assignTo(Participant* participant) ;
        Participant* getAssignedParticipant() const ;

        void makeAvailable();
        void assign();
        void start();
        void complete();
        void reject();
        void cancel();
        void escalate();

        // Prototype: same name and role, fresh CreatedState, no assignee, no listeners.
        WorkflowComponent* clone() const override;

        // Engine-only operations that bypass the State rules on purpose:
        // reopen() puts finished work back to Created for a retry loop (ProgressionResult::reopened);
        // restoreState() is used by Memento rollback.
        void reopen();
        void restoreState(WorkItemState* state, Participant* assignee);

         // Composite interface
        void add(WorkflowComponent* component) override;
        void remove(WorkflowComponent* component) override;
        void accept(WorkflowVisitor* visitor) override;

        // observer pattern
        using EventSource::notifyListeners;
        void notifyListeners(Event* event); // takes ownership of event
};

#endif