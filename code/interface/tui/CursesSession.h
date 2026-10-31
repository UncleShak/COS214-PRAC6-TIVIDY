/**
 * @file CursesSession.h
 * @brief RAII guard that owns the terminal while the ncurses front end runs.
 *
 * The constructor puts the terminal into curses mode and the destructor always restores
 * it, even when an exception unwinds the stack. Ctrl+C (SIGINT) also restores the terminal
 * before the process exits, so a crash or interrupt never leaves a broken shell behind.
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
    /** @brief Leaves curses mode (endwin) and restores the previous SIGINT handler. */
    ~CursesSession();

    /** @brief True if the terminal supports colour. */
    bool hasColour() const { return colour_; }

private:
    CursesSession(const CursesSession&) = delete;
    CursesSession& operator=(const CursesSession&) = delete;

    bool colour_;
    void (*previousHandler_)(int);
};

} // namespace ui
} // namespace tividy

#endif
