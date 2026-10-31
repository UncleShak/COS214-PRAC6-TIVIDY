/**
 * @file main.cpp
 * @brief Entry point of the TIVIDY interactive interface.
 *
 * @code
 *   tividy              ncurses interface
 *   tividy --plain      line-based interface (fallback)
 *   tividy --log FILE   write debug output to FILE
 * @endcode
 *
 * The engine comes from makeEngine(), chosen at link time (see EngineFactory.h), so this
 * file is the same for the shipped program and the development build.
 *
 * @author Shakir Alexander (Lust)
 */
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "EngineFactory.h"
#include "InterfaceController.h"
#include "Log.h"
#include "PlainFrontEnd.h"

#ifdef TIVIDY_WITH_CURSES
#include "CursesSession.h"
#include "TuiFrontEnd.h"
#endif

namespace {

void usage(std::ostream& out) {
    out << "Usage: tividy [--plain [--echo]] [--log FILE]\n"
           "  --plain      line-based interface instead of ncurses\n"
           "  --echo       with --plain, repeat each command (for scripted input)\n"
           "  --log FILE   append debug output to FILE\n"
           "  --help       show this message\n";
}

int runPlain(tividy::ui::InterfaceController& controller, bool echo) {
    return tividy::ui::PlainFrontEnd(controller, std::cin, std::cout, tividy::ui::engineLabel(), echo).run();
}

#ifdef TIVIDY_WITH_CURSES
/**
 * Every exception is caught inside the session's scope and reported only after the
 * session's destructor has restored the terminal, so the message is readable.
 */
int runCurses(tividy::ui::InterfaceController& controller) {
    std::string error;
    {
        try {
            tividy::ui::CursesSession session;
            tividy::ui::TuiFrontEnd(controller, tividy::ui::engineLabel()).run();
        } catch (const std::exception& e) {
            error = e.what();
        } catch (...) {
            error = "unknown error";
        }
    }
    if (!error.empty()) {
        tividy::ui::Log::write("fatal: " + error);
        std::cerr << "tividy: " << error << "\n"
                  << "If the screen looks wrong, type 'reset' and press Enter.\n";
        return 1;
    }
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
    bool plain = false;
    bool echo = false;
    std::string logPath;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--plain") == 0) {
            plain = true;
        } else if (std::strcmp(argv[i], "--echo") == 0) {
            echo = true;
        } else if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            logPath = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            usage(std::cout);
            return 0;
        } else {
            std::cerr << "tividy: unknown option '" << argv[i] << "'\n";
            usage(std::cerr);
            return 2;
        }
    }

    if (!logPath.empty() && !tividy::ui::Log::open(logPath)) {
        std::cerr << "tividy: cannot open log file '" << logPath << "', continuing without it\n";
    }

    std::unique_ptr<tividy::ui::EngineGateway> engine;
    try {
        engine = tividy::ui::makeEngine();
    } catch (const std::exception& e) {
        std::cerr << "tividy: the engine failed to start: " << e.what() << "\n";
        return 1;
    }

    tividy::ui::InterfaceController controller(*engine);
    controller.refresh();

#ifdef TIVIDY_WITH_CURSES
    if (!plain) {
        return runCurses(controller);
    }
#else
    if (!plain) {
        std::cerr << "tividy: built without ncurses, using --plain\n";
    }
#endif
    return runPlain(controller, echo);
}
