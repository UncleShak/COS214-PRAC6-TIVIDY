/**
 * @file TaskDecorator.h
 * @brief Decorator participant: wraps a WorkflowComponent and forwards every call to it.
 *
 * Concrete decorators override request() to add behaviour before or after the forwarded
 * call (validation, auditing, deadlines, retries) without changing WorkItem or Stage.
 * Industry-specific extras are added by wrapping, so the engine classes stay unchanged.
 *
 * Ownership: the decorator holds the component by aggregation (hollow diamond in the
 * class diagram). It does not delete the component; whoever built the structure owns it.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_TASK_DECORATOR_H
#define TIVIDY_TASK_DECORATOR_H

#include "../workflow/WorkflowComponent.h"

/** @brief Abstract decorator: a WorkflowComponent that wraps exactly one other component. */
class TaskDecorator : public WorkflowComponent {
public:
    /**
     * @param c The component to decorate (a WorkItem, a Stage or another decorator).
     * @throws std::invalid_argument if c is null.
     */
    explicit TaskDecorator(WorkflowComponent* c);
    virtual ~TaskDecorator() {}

    /** @brief Forwards to the wrapped component. Concrete decorators add behaviour around it. */
    void request() override;
    /** @brief Forwards unchanged, so visitors still reach the real work item. */
    void accept(WorkflowVisitor* v) override;
    /** @brief Forwards unchanged, so a decorated item keeps its name. */
    std::string getName() const override;
    /**
     * @brief Forwards unchanged. A decorated Stage still takes children; a decorated
     *        WorkItem still refuses them (std::logic_error).
     */
    void add(WorkflowComponent* c) override;
    /** @brief Forwards unchanged. */
    void remove(WorkflowComponent* c) override;

    /** @brief The component this decorator wraps. */
    WorkflowComponent* getComponent() const { return component; }

protected:
    WorkflowComponent* component;  ///< The wrapped component (not owned).

private:
    // A decorator wraps one specific component; copying it would silently share that component.
    TaskDecorator(const TaskDecorator&);
    TaskDecorator& operator=(const TaskDecorator&);
};

#endif
