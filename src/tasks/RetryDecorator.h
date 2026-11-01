/**
 * @file RetryDecorator.h
 * @brief ConcreteDecorator: retries work that fails, for example a call to an unreliable
 *        external system through an Adapter.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_RETRY_DECORATOR_H
#define TIVIDY_RETRY_DECORATOR_H

#include "TaskDecorator.h"

/** @brief Tries the wrapped work up to maxAttempts times before giving up. */
class RetryDecorator : public TaskDecorator {
public:
    /**
     * @param c The component to decorate.
     * @param maxAttempts Total attempts allowed, including the first. Must be at least 1.
     * @throws std::invalid_argument if maxAttempts is less than 1.
     */
    RetryDecorator(WorkflowComponent* c, int maxAttempts);

    /**
     * @brief Forwards the request, retrying after a std::exception.
     *
     * A std::logic_error (an illegal transition from the states, or a WorkflowException
     * such as failed validation or a missed deadline) is NOT retried: repeating it cannot succeed, so it is rethrown at once. Any other failure is
     * retried, and the last one is rethrown after maxAttempts.
     */
    void request() override;

    int getMaxAttempts() const { return maxAttempts; }
    /** @brief Attempts made by the most recent request(). */
    int getAttemptsUsed() const { return attemptsUsed; }

private:
    int maxAttempts;
    int attemptsUsed;
};

#endif
