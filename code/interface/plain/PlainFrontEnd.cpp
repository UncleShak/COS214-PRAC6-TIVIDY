#include "PlainFrontEnd.h"

#include <exception>
#include <istream>
#include <ostream>

#include "Log.h"

namespace tividy {
namespace ui {

PlainFrontEnd::PlainFrontEnd(InterfaceController& controller, std::istream& in, std::ostream& out,
                             const std::string& engineLabel, bool echo)
    : interpreter_(controller), in_(in), out_(out), engineLabel_(engineLabel),
      echo_(echo) {}

int PlainFrontEnd::run() {
    out_ << "TIVIDY - Se7en workflow engine  [" << engineLabel_ << "]\n"
         << "Type 'help' for commands, 'industries' to begin, 'quit' to leave.\n";

    std::string line;
    while (true) {
        out_ << interpreter_.prompt() << std::flush;
        if (!std::getline(in_, line)) {
            out_ << "\n";
            break; // end of input (Ctrl+D or end of a script)
        }
        if (echo_) {
            out_ << line << "\n";
        }

        // Team convention: catch std::exception once, here, and print what().
        try {
            Reply reply = interpreter_.run(line);
            for (std::size_t i = 0; i < reply.lines.size(); ++i) {
                out_ << reply.lines[i] << "\n";
            }
            if (!reply.status.text.empty()) {
                out_ << (reply.status.isError() ? "! " : "") << reply.status.text << "\n";
            }
            if (reply.quit) {
                break;
            }
        } catch (const std::exception& e) {
            Log::write(std::string("plain: ") + e.what());
            out_ << "! " << e.what() << "\n";
        }
    }
    out_ << "Goodbye.\n";
    return 0;
}

} // namespace ui
} // namespace tividy
