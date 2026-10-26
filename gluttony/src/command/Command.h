/**
 * @file Command.h
 * @brief Command participant: one action a participant takes on a work item, as an object.
 *
 * Turning actions into objects lets the interface, the engine and tests issue them the same
 * way, lets the Invoker (WorkflowInstance) keep a log of what was done and by whom, and keeps
 * the rules for each action inside the Receiver (WorkItem), where the State pattern enforces them.
 *
 * Undo is handled by Memento (HistoryManager restores an InstanceSnapshot), so Command has
 * no undo() of its own.
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_COMMAND_H
#define TIVIDY_COMMAND_H

#include <string>

class WorkItem;
class Participant;

/** @brief Abstract command bound to a receiver (the work item) and an actor. */
class Command {
public:
    /**
     * @param i The work item the action is performed on (the Receiver, not owned).
     * @param a The participant performing the action (not owned).
     * @throws std::invalid_argument if either is null.
     */
    Command(WorkItem* i, Participant* a);
    virtual ~Command() {}

    /**
     * @brief Carries out the action on the receiver.
     * @throws InvalidTransition if the work item's current state does not allow the action.
     */
    virtual void execute() = 0;

    /** @brief Short description for history and the interface, for example "complete 'Verify documents'". */
    virtual std::string describe() const = 0;

    WorkItem* getItem() const { return item; }
    Participant* getActor() const { return actor; }

protected:
    WorkItem* item;      ///< Receiver.
    Participant* actor;  ///< Who issued the command.
};

#endif
