/**
 * @file Command.h
 * @brief Command participant: one action a participant takes on a work item, as an object.
 *
 * Turning actions into objects lets the interface, the engine and tests issue them the same
 * way, lets the Invoker (WorkflowInstance) keep a log of what was done and by whom, and keeps
 * the rules for each action inside the Receiver (WorkItem), where the State pattern enforces them.
 *
 * A command can also be given a target: the decorated version of its work item (for example
 * DeadlineDecorator(&item)). CompleteCommand performs the work through the target, so the
 * decorators run whenever a participant completes the item.
 *
 * Undo is handled by Memento (HistoryManager restores an InstanceSnapshot), so Command has
 * no undo() of its own.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_COMMAND_H
#define TIVIDY_COMMAND_H

#include <string>

class WorkItem;
class Participant;
class WorkflowComponent;

/** @brief Abstract command bound to a receiver (the work item) and an actor. */
class Command {
public:
    /**
     * @param i The work item the action is performed on (the Receiver, not owned).
     * @param a The participant performing the action (not owned).
     * @param target The decorated version of i, if it has decorators (not owned).
     *               If null, the work item itself is used.
     * @throws std::invalid_argument if i or a is null.
     */
    Command(WorkItem* i, Participant* a, WorkflowComponent* target = nullptr);
    virtual ~Command() {}

    /**
     * @brief Carries out the action on the receiver.
     * @throws std::logic_error if the work item's current state does not allow the action,
     *         or a decorator refuses it (ValidationFailed, DeadlineExceeded).
     */
    virtual void execute() = 0;

    /** @brief Short description for history and the interface, for example "Thandi started 'Verify documents'". */
    virtual std::string describe() const = 0;

    WorkItem* getItem() const { return item; }
    Participant* getActor() const { return actor; }
    WorkflowComponent* getTarget() const { return target; }

protected:
    /**
     * @brief Only the participant the item is assigned to may start or complete it.
     * @throws std::logic_error if the actor is not the item's assigned participant.
     */
    void requireAssignee() const;

    WorkItem* item;              ///< Receiver.
    Participant* actor;          ///< Who issued the command.
    WorkflowComponent* target;   ///< The item, or its outermost decorator.
};

#endif
