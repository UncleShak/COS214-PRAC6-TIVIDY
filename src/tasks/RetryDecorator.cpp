#include "RetryDecorator.h"

#include "WorkflowException.h"

#include <exception>
#include <stdexcept>

RetryDecorator::RetryDecorator(WorkflowComponent* c, int maxAttempts)
    : TaskDecorator(c), maxAttempts(maxAttempts), attemptsUsed(0) {
    if (maxAttempts < 1) {
        throw std::invalid_argument("RetryDecorator needs at least one attempt");
    }
}

void RetryDecorator::request() {
    attemptsUsed = 0;
    while (true) {
        ++attemptsUsed;
        try {
            TaskDecorator::request();
            return;
        } catch (const std::logic_error&) {
            // An illegal transition (from the states) or a failed rule (ValidationFailed,
            // DeadlineExceeded) will fail again: never retry it.
            throw;
        } catch (const std::exception&) {
            if (attemptsUsed >= maxAttempts) {
                throw;
            }
        }
    }
}
