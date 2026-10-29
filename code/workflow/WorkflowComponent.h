#ifndef WORKFLOW_COMPONENT_H
#define WORKFLOW_COMPONENT_H

// The generic base for individual tasks and a group of tasks

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
};

#endif