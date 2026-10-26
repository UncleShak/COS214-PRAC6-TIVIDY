/**
 * @file AuditDecorator.h
 * @brief ConcreteDecorator: records an audit entry before and after the work, including failures.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_AUDIT_DECORATOR_H
#define TIVIDY_AUDIT_DECORATOR_H

#include "TaskDecorator.h"

class History;

/** @brief Writes "started", then "completed" or "failed: reason", to a History. */
class AuditDecorator : public TaskDecorator {
public:
    /**
     * @param c The component to decorate.
     * @param history Where entries are written (not owned).
     * @throws std::invalid_argument if history is null.
     */
    AuditDecorator(WorkflowComponent* c, History* history);

    /**
     * @brief Records the start, forwards the request, then records the outcome.
     * Any exception from the wrapped component is recorded and then rethrown unchanged.
     */
    void request() override;

private:
    History* history;
};

#endif
