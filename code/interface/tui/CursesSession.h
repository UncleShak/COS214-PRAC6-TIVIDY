/**
 * @file CursesSession.h
 * @brief RAII guard that owns the terminal while the ncurses front end runs.
 *
 * The constructor puts the terminal into curses mode and the destructor always restores
 * it, even when an exception unwinds the stack. Ctrl+C (SIGINT), SIGTERM, SIGHUP and a crash
 * (SIGSEGV, SIGABRT) also restore the terminal before the process ends, so nothing leaves a
 * broken shell behind.
 *
 * Nothing may write to std::cout or std::cerr while a CursesSession exists. Use Log.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_CURSES_SESSION_H
#define TIVIDY_CURSES_SESSION_H

namespace tividy {
namespace ui {

/** @brief Owns curses mode for its lifetime. Only one may exist at a time. Not copyable. */
class CursesSession {
public:
    /**
     * @brief Enters curses mode: locale, cbreak, noecho, keypad, hidden cursor, colours.
     * @throws std::runtime_error if the terminal cannot be initialised or a session exists
     */
    CursesSession();
    /** @brief Leaves curses mode (endwin) and restores the previous signal handlers. */
    ~CursesSession();

    /** @brief True if the terminal supports colour. */
    bool hasColour() const { return colour_; }

private:
    CursesSession(const CursesSession&) = delete;
    CursesSession& operator=(const CursesSession&) = delete;

    enum { kSignalCount = 5 };   ///< SIGINT, SIGTERM, SIGHUP, SIGSEGV, SIGABRT

    bool colour_;
    void (*previousHandlers_[kSignalCount])(int);
};

} // namespace ui
} // namespace tividy

#endif
