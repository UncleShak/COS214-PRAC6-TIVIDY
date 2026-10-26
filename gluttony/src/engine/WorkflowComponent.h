/**
 * @file WorkflowComponent.h
 * @brief Component of the Decorator (and Composite) pattern: the common abstraction for work.
 *
 * STAND-IN for the engine class owned by Pride. It declares exactly the operations the
 * Practical 6 class diagram gives WorkflowComponent, plus request() and getName()
 * (see docs/decisions-gluttony.md, D-G1 and D-G6). Replace with Pride's header when it lands;
 * the decorators only rely on the operations declared here.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_WORKFLOW_COMPONENT_H
#define TIVIDY_WORKFLOW_COMPONENT_H

#include <string>

class WorkflowVisitor;
class WorkflowIterator;

/**
 * @brief Abstract node in a workflow: a single WorkItem, a Stage of work, or a decorator.
 *
 * Every operation is pure virtual so that a TaskDecorator can stand in anywhere a
 * WorkflowComponent is expected (Decorator transparency).
 */
class WorkflowComponent {
public:
    virtual ~WorkflowComponent() {}

    /** @brief Performs the work this component represents. */
    virtual void request() = 0;

    /** @brief Performs the work through an access-controlled path (used by the Proxy pattern). */
    virtual void proxyRequest() = 0;

    /**
     * @brief Accepts a visitor (Visitor pattern).
     * @param v The visitor that will inspect this component.
     */
    virtual void accept(WorkflowVisitor* v) = 0;

    /**
     * @brief Creates an iterator over this component's children (Iterator pattern).
     * @return A new iterator owned by the caller, or nullptr for a leaf.
     */
    virtual WorkflowIterator* createIterator() = 0;

    /** @brief Human-readable name, used in audit entries and messages. */
    virtual std::string getName() const = 0;
};

#endif
