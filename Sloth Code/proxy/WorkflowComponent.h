/**
 * @file WorkflowComponent.h
 * @brief Base Component interface in the Composite, Decorator, and Proxy patterns.
 *
 * Subject in the Proxy pattern.
 * Leaf: WorkItem. Composite: Stage.
 * Decorator: TaskDecorator. Proxy: AccessControlProxy.
 */
#ifndef TIVIDY_WORKFLOW_COMPONENT_H
#define TIVIDY_WORKFLOW_COMPONENT_H

#include <string>

class WorkflowVisitor;

class WorkflowComponent {
public:
    virtual ~WorkflowComponent() = default;

    /**
     * @brief Unique identifier or name of the component.
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Executes or completes the unit of work.
     * Guarded by AccessControlProxy, decorated by TaskDecorator.
     */
    virtual void request() = 0;

    /**
     * @brief Adds a child component (Composite pattern).
     */
    virtual void add(WorkflowComponent* component) = 0;

    /**
     * @brief Removes a child component (Composite pattern).
     */
    virtual void remove(WorkflowComponent* component) = 0;

    /**
     * @brief Accepts a visitor for inspection/reporting (Visitor pattern).
     */
    virtual void accept(WorkflowVisitor* visitor) = 0;
};

#endif // TIVIDY_WORKFLOW_COMPONENT_H
