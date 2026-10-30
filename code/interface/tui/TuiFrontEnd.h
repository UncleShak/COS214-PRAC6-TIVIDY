/**
 * @file TuiFrontEnd.h
 * @brief The ncurses front end: draws the panes and turns key presses into controller calls.
 *
 * No engine or workflow logic lives here. Everything the user does goes through
 * InterfaceController, and everything drawn comes from its SessionView.
 *
 * Skeleton (30 Oct): header, industry list, detail pane, status bar, arrow-key selection,
 * resize handling, the too-small screen and quit. The other screens from
 * docs/tui-plan.md section 4 follow.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_TUI_FRONT_END_H
#define TIVIDY_TUI_FRONT_END_H

#include <string>

#include <curses.h>

#include "InterfaceController.h"
#include "Layout.h"
#include "Theme.h"

namespace tividy {
namespace ui {

/** @brief Event loop of the ncurses interface. Requires an active CursesSession. */
class TuiFrontEnd {
public:
    /** @param engineLabel shown in the header so a fake engine is never mistaken for the real one */
    TuiFrontEnd(InterfaceController& controller, const std::string& engineLabel);

    /** @brief Runs until the user quits. */
    void run();

private:
    void draw();
    void drawTooSmall();
    void drawHeader();
    void drawList();
    void drawDetail();
    void drawStatus();

    /** @brief Box with a title; gold border when the pane has focus. */
    void frame(WINDOW* window, const std::string& title, bool focused);
    /** @brief Writes @p text at (y, x), cut to fit the window width minus @p margin. */
    static void put(WINDOW* window, int y, int x, const std::string& text, Tone tone, int margin = 1);

    InterfaceController& controller_;
    std::string engineLabel_;
    Layout layout_;
    int selected_;
    bool running_;
};

} // namespace ui
} // namespace tividy

#endif
