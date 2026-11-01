#include "InterruptListener.h"
#include "Event.h"
#include "WorkflowInstance.h"

InterruptListener::InterruptListener(WorkflowInstance& target, const WorkflowDefinition::InterruptRule& r)
    : instance(target), rule(r) {}

void InterruptListener::onEvent(const Event& event) {
    if (event.getType() == rule.eventType) instance.interrupt(rule, event.getDescription());
}
