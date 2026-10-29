/**
 * @file ValidationDecorator.h
 * @brief ConcreteDecorator: refuses to perform the work unless its constraint is satisfied.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_VALIDATION_DECORATOR_H
#define TIVIDY_VALIDATION_DECORATOR_H

#include "TaskDecorator.h"

class Constraint;

/** @brief Checks a Constraint before forwarding request(). */
class ValidationDecorator : public TaskDecorator {
public:
    /**
     * @param c The component to decorate.
     * @param constraint The rule to check (not owned).
     * @throws std::invalid_argument if constraint is null.
     */
    ValidationDecorator(WorkflowComponent* c, const Constraint* constraint);

    /**
     * @brief Performs the work only if the constraint holds.
     * @throws ValidationFailed if the constraint is not satisfied; the wrapped work does not run.
     */
    void request() override;

private:
    const Constraint* constraint;
};

#endif
