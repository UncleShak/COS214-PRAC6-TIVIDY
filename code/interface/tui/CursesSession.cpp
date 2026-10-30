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

/**
 * Ctrl+C while ncurses is active: put the terminal back first, then exit. endwin() is not
 * strictly async-signal-safe, but it is the standard way to leave curses on SIGINT and the
 * process exits straight afterwards.
 */
void restoreAndExit(int) {
    endwin();
    _exit(130);
}

} // namespace

CursesSession::CursesSession() : colour_(false), previousHandler_(SIG_DFL) {
    if (activeScreen != 0) {
        throw std::runtime_error("a curses session is already active");
    }

    std::setlocale(LC_ALL, ""); // before curses starts, for UTF-8 in Windows Terminal

    activeScreen = newterm(0, stdout, stdin);
    if (activeScreen == 0) {
        throw std::runtime_error("cannot initialise the terminal (is TERM set?)");
    }
    set_term(activeScreen);
    previousHandler_ = std::signal(SIGINT, restoreAndExit);

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
    std::signal(SIGINT, previousHandler_);
    Log::write("curses session ended");
}

} // namespace ui
} // namespace tividy
