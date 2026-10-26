/**
 * @file WorkflowIterator.h
 * @brief STAND-IN for the Iterator interface (owned by another member). Only declared so
 *        that createIterator() has a return type the decorators can forward.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_WORKFLOW_ITERATOR_H
#define TIVIDY_WORKFLOW_ITERATOR_H

class WorkflowComponent;

/** @brief External iterator over workflow components. */
class WorkflowIterator {
public:
    virtual ~WorkflowIterator() {}
    virtual void first() = 0;
    virtual void next() = 0;
    virtual bool isDone() const = 0;
    virtual WorkflowComponent* currentItem() const = 0;
};

#endif
