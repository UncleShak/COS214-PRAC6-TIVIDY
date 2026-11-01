/**
 * @file PlainFrontEnd.h
 * @brief Line-based interface (tividy --plain): type a command, read the answer.
 *
 * The fallback for the demo if ncurses misbehaves, and the simplest way to drive the engine.
 * All commands are handled by LineInterpreter; this class only reads lines and prints replies.
 * Works on any std::istream / std::ostream, so tests can feed it a script.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_PLAIN_FRONT_END_H
#define TIVIDY_PLAIN_FRONT_END_H

#include <iosfwd>
#include <string>

#include "InterfaceController.h"
#include "LineInterpreter.h"

namespace tividy {
namespace ui {

/** @brief Read-run-print loop over a LineInterpreter. */
class PlainFrontEnd {
public:
    /**
     * @param echo also print each command after the prompt (useful when input is a script)
     */
    PlainFrontEnd(InterfaceController& controller, std::istream& in, std::ostream& out,
                  const std::string& engineLabel, bool echo = false);

    /** @brief Runs until "quit" or end of input. @return the process exit code (0) */
    int run();

private:
    LineInterpreter interpreter_;
    std::istream& in_;
    std::ostream& out_;
    std::string engineLabel_;
    bool echo_;
};

} // namespace ui
} // namespace tividy

#endif
