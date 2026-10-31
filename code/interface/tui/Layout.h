/**
 * @file Layout.h
 * @brief The ncurses windows of the front end, created from LayoutGeometry.
 *
 * Rebuilt from scratch on every terminal resize. Below the minimum size no windows exist
 * and the front end shows a "terminal too small" message instead. The menu window is null
 * when the terminal is too narrow for it (see Geometry::kMenuMinCols).
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_LAYOUT_H
#define TIVIDY_LAYOUT_H

#include <curses.h>

#include "LayoutGeometry.h"

namespace tividy {
namespace ui {

/** @brief Owns the pane windows. Not copyable. */
class Layout {
public:
    static const int kMinCols = Geometry::kMinCols;
    static const int kMinRows = Geometry::kMinRows;

    Layout();
    ~Layout();

    /** @brief Recreates every window for the current terminal size (LINES x COLS). */
    void rebuild();

    bool tooSmall() const { return geometry_.tooSmall; }
    const Geometry& geometry() const { return geometry_; }

    WINDOW* title() const { return title_; }
    WINDOW* calendar() const { return calendar_; }
    WINDOW* menu() const { return menu_; }
    WINDOW* list() const { return list_; }
    WINDOW* detail() const { return detail_; }
    WINDOW* body() const { return body_; }
    WINDOW* ticker() const { return ticker_; }
    WINDOW* status() const { return status_; }

private:
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    void destroy();
    static WINDOW* open(const Rect& r);

    Geometry geometry_;
    WINDOW* title_;
    WINDOW* calendar_;
    WINDOW* menu_;
    WINDOW* list_;
    WINDOW* detail_;
    WINDOW* body_;
    WINDOW* ticker_;
    WINDOW* status_;
};

} // namespace ui
} // namespace tividy

#endif
