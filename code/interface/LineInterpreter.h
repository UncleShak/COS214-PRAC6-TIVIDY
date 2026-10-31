/**
 * @file LineInterpreter.h
 * @brief Reads one typed command line and carries it out through the InterfaceController.
 *
 * Shared by the plain front end (every line typed) and the ncurses front end (its ':' bar),
 * so both understand exactly the same commands. Pure text in, text out: no I/O here.
 *
 * Commands (type "help" in the program for the same list):
 * @code
 *   industries                      list industries          industry <n|id>   choose one
 *   instances                       list instances           open <n|id>       choose one
 *   start key=value ...             start an instance        people            list participants
 *   as <participant>                act as someone else
 *   items                           all work items           work              available work only
 *   show <item>                     one item in detail
 *   assign <item> [to <person>]     begin <item>             complete <item>
 *   reject-work <item> <reason>     cancel <item>            escalate <item>
 *   approvals                       approve <id>             reject <id> <reason>
 *   advance <units>                 move the clock
 *   history [newest] [text]         report                   help              quit
 * @endcode
 *
 * An <item> is its number from "items" or its name (any case, spaces allowed).
 * A <person> is their number from "people", their ID or their name.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_LINE_INTERPRETER_H
#define TIVIDY_LINE_INTERPRETER_H

#include <string>
#include <vector>

#include "InterfaceController.h"
#include "ViewModels.h"

namespace tividy {
namespace ui {

/** @brief What one command produced: lines to print, a status line, and whether to quit. */
struct Reply {
    std::vector<std::string> lines;
    StatusMessage status;
    bool quit;

    Reply() : quit(false) {}
};

/** @brief Parses typed commands and runs them through the controller. */
class LineInterpreter {
public:
    /** @param controller must outlive the interpreter */
    explicit LineInterpreter(InterfaceController& controller);

    /** @brief Runs one typed line. Never throws for bad input; reports it in Reply::status. */
    Reply run(const std::string& line);

    /** @brief Prompt showing where the user is, e.g. "tividy fake-a/FAKE-0001 (Alex)> ". */
    std::string prompt() const;

    /** @brief The command list shown by "help". */
    static std::vector<std::string> helpText();

private:
    Reply listIndustries() const;
    Reply chooseIndustry(const std::vector<std::string>& args);
    Reply listInstances() const;
    Reply openInstance(const std::vector<std::string>& args);
    Reply start(const std::vector<std::string>& args);
    Reply listPeople() const;
    Reply actAs(const std::vector<std::string>& args);
    Reply listItems(bool availableOnly) const;
    Reply showItem(const std::vector<std::string>& args) const;
    Reply workAction(WorkAction action, const std::vector<std::string>& args);
    Reply listApprovals() const;
    Reply decideApproval(bool approve, const std::vector<std::string>& args);
    Reply advance(const std::vector<std::string>& args);
    Reply history(const std::vector<std::string>& args);
    Reply report();

    /**
     * @brief Finds the work item named by the first words of @p args.
     * @param used set to how many words named the item
     * @return the item ID, or "" if none matches
     */
    std::string resolveItem(const std::vector<std::string>& args, std::size_t& used) const;
    /** @brief Participant ID from a number, ID or name (words joined), or "" if none matches. */
    std::string resolvePerson(const std::vector<std::string>& words) const;
    /** @brief Lines describing the last outcome: state change, routing, what became available. */
    std::vector<std::string> outcomeLines() const;

    InterfaceController& controller_;
};

} // namespace ui
} // namespace tividy

#endif
