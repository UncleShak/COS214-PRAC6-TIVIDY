#ifndef TIVIDY_INTERRUPT_LISTENER_H
#define TIVIDY_INTERRUPT_LISTENER_H

#include "EventListener.h"
#include "WorkflowDefinition.h"

class WorkflowInstance;

// Observer (ConcreteObserver): listens to one instance and, when an event of its rule's type
// arrives, asks the instance to cancel the rule's unfinished work (CancelCommand) and open
// the rule's response work. Example: Dune's WORM_SIGN cancels the harvest and opens Evacuate.
class InterruptListener : public EventListener {
public:
    InterruptListener(WorkflowInstance& instance, const WorkflowDefinition::InterruptRule& rule);
    void onEvent(const Event& event) override;
    const WorkflowDefinition::InterruptRule& getRule() const { return rule; }

private:
    WorkflowInstance& instance;
    WorkflowDefinition::InterruptRule rule;
};

#endif
