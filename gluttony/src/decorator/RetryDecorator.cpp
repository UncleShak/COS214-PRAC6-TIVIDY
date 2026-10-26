#include "RetryDecorator.h"

#include "../engine/WorkflowException.h"

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
        } catch (const WorkflowException&) {
            throw;  // A refused action will be refused again: never retry it.
        } catch (const std::exception&) {
            if (attemptsUsed >= maxAttempts) {
                throw;
            }
        }
    }
}
