/**
 * @file WorkflowException.h
 * @brief Errors raised when the engine refuses work because a workflow rule was broken.
 *
 * WorkflowException derives from std::logic_error, the same type the WorkItemState classes
 * throw for illegal transitions. Code that wants to catch "the workflow refused this" can
 * catch std::logic_error and handle both cases.
 *
 * The decorators throw the typed subclasses below (ValidationFailed, DeadlineExceeded), and
 * RetryDecorator rethrows them rather than retrying, because a refused action will be refused
 * again. The state classes currently throw raw std::logic_error, which RetryDecorator does
 * treat as retryable; Phase 4 migrates them to InvalidTransition so the rethrow path covers
 * them too.
 *
 * A refused action is an exception. A normal outcome (an approval being rejected, a request
 * being escalated) is never an exception: it is a status the caller reads.
 *
 * @author N'des Junior Lungwangu (Gluttony) and Leul Tamiru (Wrath)
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

/** @brief The operation is not legal in the object's current state. */
class InvalidTransition : public WorkflowException {
public:
    explicit InvalidTransition(const std::string& message) : WorkflowException(message) {}
};

/** @brief A precondition (Constraint) did not hold, so the work was not performed. */
class ValidationFailed : public WorkflowException {
public:
    explicit ValidationFailed(const std::string& message) : WorkflowException(message) {}
};

/** @brief The participant does not hold the role the operation needs. */
class AccessDenied : public WorkflowException {
public:
    explicit AccessDenied(const std::string& message) : WorkflowException(message) {}
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
