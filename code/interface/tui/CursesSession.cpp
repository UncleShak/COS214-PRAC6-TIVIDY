#include "CursesSession.h"

#include <clocale>
#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>

#include <curses.h>

#include "Log.h"
#include "Theme.h"

namespace tividy {
namespace ui {

namespace {

SCREEN* activeScreen = 0;

/** Signals that would leave the terminal in curses mode if nobody restored it. */
const int kGuardedSignals[] = {SIGINT, SIGTERM, SIGHUP, SIGSEGV, SIGABRT};
const int kGuardedCount = 5;

/**
 * Ctrl+C, a kill, a closed window or a crash while ncurses is active: put the terminal back
 * first. endwin() is not strictly async-signal-safe, but it is the standard way to leave
 * curses here and the process ends straight afterwards. Interrupts exit with 128 + signal;
 * a crash is re-raised with the default action so it still produces its usual report.
 */
void restoreAndExit(int sig) {
    endwin();
    if (sig == SIGSEGV || sig == SIGABRT) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
    _exit(128 + sig);
}

} // namespace

CursesSession::CursesSession() : colour_(false) {
    for (int i = 0; i < kSignalCount; ++i) {
        previousHandlers_[i] = SIG_DFL;
    }
    if (activeScreen != 0) {
        throw std::runtime_error("a curses session is already active");
    }

    std::setlocale(LC_ALL, ""); // before curses starts, for UTF-8 in Windows Terminal

    activeScreen = newterm(0, stdout, stdin);
    if (activeScreen == 0) {
        throw std::runtime_error("cannot initialise the terminal (is TERM set?)");
    }
    set_term(activeScreen);
    for (int i = 0; i < kGuardedCount; ++i) {
        previousHandlers_[i] = std::signal(kGuardedSignals[i], restoreAndExit);
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25); // Esc closes dialogs without the default one-second wait

    if (has_colors()) {
        start_color();
        use_default_colors();
        colour_ = true;
    }
    Theme::init(colour_);
    Log::write("curses session started");
}

CursesSession::~CursesSession() {
    endwin();
    delscreen(activeScreen);
    activeScreen = 0;
    for (int i = 0; i < kGuardedCount; ++i) {
        std::signal(kGuardedSignals[i], previousHandlers_[i]);
    }
    Log::write("curses session ended");
}

} // namespace ui
} // namespace tividy
