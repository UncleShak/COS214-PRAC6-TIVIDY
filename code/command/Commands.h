/**
 * @file Commands.h
 * @brief The ConcreteCommands. Each binds its receiver (WorkItem) to receiver operations.
 *
 * | Command         | Receiver operations                        | Lifecycle move                       |
 * |-----------------|--------------------------------------------|--------------------------------------|
 * | AssignCommand   | assign(), assignTo(assignee)               | Available or Escalated -> Assigned   |
 * | StartCommand    | start()                                    | Assigned -> Started                  |
 * | CompleteCommand | request() on the target, which completes   | Started -> Completed                 |
 * | RejectCommand   | reject()                                   | Started -> Rejected                  |
 * | CancelCommand   | cancel()                                   | Available, Assigned, Started, Escalated -> Cancelled |
 * | EscalateCommand | escalate()                                 | Available -> Escalated               |
 *
 * The WorkItemState classes decide whether each move is legal and throw std::logic_error
 * if it is not. The commands add two rules of their own: only the assignee may start or
 * complete the work, and the assignee's workload is kept up to date for
 * WorkloadBasedAssignment.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_COMMANDS_H
#define TIVIDY_COMMANDS_H

#include "Command.h"

#include <string>

/** @brief Gives an available work item to an assignee and adds it to their workload. */
class AssignCommand : public Command {
public:
    /**
     * @param i The work item.
     * @param a Who is assigning it (for example a supervisor or the routing engine).
     * @param assignee Who receives the work.
     * @throws std::invalid_argument if assignee is null.
     */
    AssignCommand(WorkItem* i, Participant* a, Participant* assignee);
    void execute() override;
    std::string describe() const override;
    Participant* getAssignee() const { return assignee; }

private:
    Participant* assignee;
};

/** @brief The assignee starts the work. */
class StartCommand : public Command {
public:
    StartCommand(WorkItem* i, Participant* a);
    void execute() override;
    std::string describe() const override;
};

/**
 * @brief The assignee finishes the work.
 *
 * Completion goes through the target (the item's decorators, if any), so validation,
 * deadlines, auditing and retries apply. If a decorator refuses, the item stays Started.
 */
class CompleteCommand : public Command {
public:
    /** @param target The decorated item, or null if the item has no decorators. */
    CompleteCommand(WorkItem* i, Participant* a, WorkflowComponent* target = nullptr);
    void execute() override;
    std::string describe() const override;
};

/** @brief Rejects started work. The command keeps the reason, since WorkItem::reject() takes none. */
class RejectCommand : public Command {
public:
    /** @param reason Why the work was rejected; shown in history and the interface. */
    RejectCommand(WorkItem* i, Participant* a, const std::string& reason);
    void execute() override;
    std::string describe() const override;
    const std::string& getReason() const { return reason; }

private:
    std::string reason;
};

/** @brief Cancels work that has not finished and frees the assignee, if there is one. */
class CancelCommand : public Command {
public:
    CancelCommand(WorkItem* i, Participant* a);
    void execute() override;
    std::string describe() const override;
};

/**
 * @brief Moves available work nobody has acted on to the escalation path.
 *
 * Issued by the escalation chain (Chain of Responsibility), for example after it catches
 * DeadlineExceeded. An escalated item can then be assigned again with AssignCommand.
 */
class EscalateCommand : public Command {
public:
    EscalateCommand(WorkItem* i, Participant* a);
    void execute() override;
    std::string describe() const override;
};

#endif
