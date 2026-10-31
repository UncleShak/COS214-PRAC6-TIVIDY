#ifndef WORKFLOW_COMPONENT_H
#define WORKFLOW_COMPONENT_H

// The generic base for individual tasks and a group of tasks

#include <stdexcept>
#include <string>

class WorkflowVisitor;

class WorkflowComponent {
    public:
        virtual ~WorkflowComponent() = default;
        virtual std::string getName() const = 0;

        // Carries out this piece of work. For a WorkItem this completes it; a Stage
        // forwards it to its children. Decorators (TaskDecorator) add behaviour around it.
        virtual void request() = 0;

        // composite methods
        virtual void add(WorkflowComponent* component)= 0;
        virtual void remove(WorkflowComponent* component)= 0;

        //visitor methods
        virtual void accept(WorkflowVisitor* visitor) = 0;

        // Proxy: asks whether `action` ("assign", "start", "complete", "reject", "cancel",
        // "escalate") may happen. Real work allows everything; AccessControlProxy throws
        // AccessDenied when its AccessPolicy refuses. Commands call it before acting.
        virtual void authorize(const std::string& action) const { (void)action; }

        // Prototype: a deep copy with fresh runtime state, so every WorkflowInstance gets its own
        // tree. WorkItem and Stage override it; anything else cannot sit in a definition's tree.
        virtual WorkflowComponent* clone() const {
            throw std::logic_error("'" + getName() + "' cannot be copied into a workflow instance");
        }
};

#endif