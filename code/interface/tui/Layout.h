/**
 * @file Layout.h
 * @brief The ncurses windows (header, list, detail, body, status) and their sizes.
 *
 * @code
 *   row 0            header  (full width)
 *   rows 1..H-3      list (40%) | detail (60%)     or     body (full width)
 *   rows H-2..H-1    status: key hints, then the last message
 * @endcode
 *
 * The body window covers the same rows as list + detail. A screen draws either the two
 * panes (pickers, dashboard) or the body (history, reports), never both.
 *
 * Rebuilt from scratch on every terminal resize. Below the minimum size no windows exist
 * and the front end shows a "terminal too small" message instead.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_LAYOUT_H
#define TIVIDY_LAYOUT_H

#include <curses.h>

namespace tividy {
namespace ui {

/** @brief Owns the pane windows. Not copyable. */
class Layout {
public:
    static const int kMinCols = 100;
    static const int kMinRows = 30;

    Layout();
    ~Layout();

    /** @brief Recreates every window for the current terminal size (LINES x COLS). */
    void rebuild();
    /** @brief True if the terminal is below kMinCols x kMinRows (windows are then null). */
    bool tooSmall() const { return tooSmall_; }

    WINDOW* header() const { return header_; }
    WINDOW* list() const { return list_; }
    WINDOW* detail() const { return detail_; }
    WINDOW* body() const { return body_; }
    WINDOW* status() const { return status_; }

private:
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    void destroy();

    WINDOW* header_;
    WINDOW* list_;
    WINDOW* detail_;
    WINDOW* body_;
    WINDOW* status_;
    bool tooSmall_;
};

} // namespace ui
} // namespace tividy

#endif
