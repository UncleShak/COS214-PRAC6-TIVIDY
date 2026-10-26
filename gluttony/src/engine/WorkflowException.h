/**
 * @file WorkflowException.h
 * @brief Exceptions raised when an operation is not allowed. Wrath's invalid-action handling
 *        can catch these as WorkflowException.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_WORKFLOW_EXCEPTION_H
#define TIVIDY_WORKFLOW_EXCEPTION_H

#include <stdexcept>
#include <string>

/** @brief Base class for every error the workflow engine reports. */
class WorkflowException : public std::runtime_error {
public:
    explicit WorkflowException(const std::string& message) : std::runtime_error(message) {}
};

/** @brief An action was attempted that the work item's current state does not allow. */
class InvalidTransition : public WorkflowException {
public:
    explicit InvalidTransition(const std::string& message) : WorkflowException(message) {}
};

/** @brief A ValidationDecorator's constraint was not met, so the work was not performed. */
class ValidationFailed : public WorkflowException {
public:
    explicit ValidationFailed(const std::string& message) : WorkflowException(message) {}
};

/** @brief A DeadlineDecorator's due time has passed; the work should be escalated. */
class DeadlineExceeded : public WorkflowException {
public:
    explicit DeadlineExceeded(const std::string& message) : WorkflowException(message) {}
};

#endif
