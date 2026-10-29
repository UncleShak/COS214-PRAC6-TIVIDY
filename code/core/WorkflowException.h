/**
 * @file WorkflowException.h
 * @brief Errors raised when the engine refuses work because a workflow rule was broken.
 *
 * WorkflowException derives from std::logic_error, the same type the WorkItemState classes
 * throw for illegal transitions. Code that wants to catch "the workflow refused this" can
 * catch std::logic_error and handle both cases. RetryDecorator relies on this: it never
 * retries a logic_error, because a refused action will be refused again.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_WORKFLOW_EXCEPTION_H
#define TIVIDY_WORKFLOW_EXCEPTION_H

#include <stdexcept>
#include <string>

/** @brief Base type for work the engine refuses because of a workflow rule. */
class WorkflowException : public std::logic_error {
public:
    explicit WorkflowException(const std::string& message) : std::logic_error(message) {}
};

/** @brief A precondition (Constraint) did not hold, so the work was not performed. */
class ValidationFailed : public WorkflowException {
public:
    explicit ValidationFailed(const std::string& message) : WorkflowException(message) {}
};

/**
 * @brief The work's due time has passed, so it was not performed.
 *
 * The escalation chain (Chain of Responsibility) catches this and passes the work item
 * to the next level, for example with an EscalateCommand.
 */
class DeadlineExceeded : public WorkflowException {
public:
    explicit DeadlineExceeded(const std::string& message) : WorkflowException(message) {}
};

#endif
