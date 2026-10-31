/**
 * @file CommandLog.h
 * @brief Invoker side of the Command pattern, ready for WorkflowInstance to use.
 *
 * The class diagram makes WorkflowInstance the Invoker (it aggregates Commands). Pride owns
 * WorkflowInstance, so this small class holds the invoking logic: WorkflowInstance can own a
 * CommandLog and call run(). It executes a command, keeps the ones that succeeded in order,
 * and leaves refused commands out of the log.
 *
 * @author N'des Junior Lungwangu (Gluttony)
 */
#ifndef TIVIDY_COMMAND_LOG_H
#define TIVIDY_COMMAND_LOG_H

#include <memory>
#include <string>
#include <vector>

class Command;

/** @brief Executes commands and remembers the ones that succeeded. */
class CommandLog {
public:
    /**
     * @brief Executes a command and, if it succeeds, keeps it in the log.
     * @param command The command to run. The log takes ownership.
     * @throws Whatever the command throws (for example std::logic_error from the states); the failed
     *         command is deleted and not logged.
     */
    void run(Command* command);

    /** @brief Number of commands that have succeeded. */
    std::size_t size() const { return done.size(); }

    /** @brief Descriptions of every successful command, oldest first. */
    std::vector<std::string> describeAll() const;

        // Drop commands whose state changes have been undone; audit history is separate.
    void truncate(std::size_t count);


private:
    std::vector<std::unique_ptr<Command>> done;
};

#endif
