/**
 * @file Commands.h
 * @brief The five ConcreteCommands from the class diagram. Each binds its receiver
 *        (WorkItem) to one receiver operation.
 *
 * | Command         | Receiver operation          | Lifecycle move                 |
 * |-----------------|-----------------------------|--------------------------------|
 * | AssignCommand   | WorkItem::assign(assignee)  | Available -> Assigned          |
 * | StartCommand    | WorkItem::start(actor)      | Assigned -> Started            |
 * | CompleteCommand | WorkItem::complete(actor)   | Started -> Completed           |
 * | RejectCommand   | WorkItem::reject(actor, r)  | Assigned or Started -> Rejected |
 * | CancelCommand   | WorkItem::cancel(actor)     | any unfinished -> Cancelled    |
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_COMMANDS_H
#define TIVIDY_COMMANDS_H

#include "Command.h"

#include <string>

/** @brief Gives an available work item to an assignee. */
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

/** @brief The assignee finishes the work. */
class CompleteCommand : public Command {
public:
    CompleteCommand(WorkItem* i, Participant* a);
    void execute() override;
    std::string describe() const override;
};

/** @brief Rejects the work, recording why. */
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

/** @brief Cancels work that has not finished. */
class CancelCommand : public Command {
public:
    CancelCommand(WorkItem* i, Participant* a);
    void execute() override;
    std::string describe() const override;
};

#endif
