#ifndef WORK_ITEM_H
#define WORK_ITEM_H

// Leaf node and is a single unit of work
#include <string>
#include "../observer/EventSource.h"
#include "WorkflowComponent.h"
#include "../state/WorkItemState.h"

class Participant;
class WorkflowVisitor;

class WorkItem : public WorkflowComponent, public EventSource{
    private:
    std::string itemName;
    std::string requiredRole;
    WorkItemState* currentState;
    Participant* assignedParticipant;

    WorkItem(const WorkItem&);
    WorkItem& operator=(const WorkItem&);

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

         // Composite interface
        void add(WorkflowComponent* component) override;
        void remove(WorkflowComponent* component) override;
        void accept(WorkflowVisitor* visitor) override;

        // observer pattern
        void notifyListeners(Event* event);
};

#endif