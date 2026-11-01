/**
 * @file Layout.h
 * @brief The ncurses windows of the front end, created from LayoutGeometry.
 *
 * Rebuilt from scratch on every terminal resize. Below the minimum size no windows exist
 * and the front end shows a "make the window bigger" message instead. The dashboard and
 * the other screens cover the same rows, so drawing either set repaints every cell.
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

    WINDOW* header() const { return windows_[kHeader]; }
    WINDOW* route() const { return windows_[kRoute]; }
    WINDOW* menu() const { return windows_[kMenu]; }
    WINDOW* list() const { return windows_[kList]; }
    WINDOW* detail() const { return windows_[kDetail]; }
    WINDOW* pageMenu() const { return windows_[kPageMenu]; }
    WINDOW* pageList() const { return windows_[kPageList]; }
    WINDOW* pageDetail() const { return windows_[kPageDetail]; }
    WINDOW* body() const { return windows_[kBody]; }
    WINDOW* announce() const { return windows_[kAnnounce]; }
    WINDOW* footer() const { return windows_[kFooter]; }

private:
    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    enum { kHeader, kRoute, kMenu, kList, kDetail, kPageMenu, kPageList, kPageDetail, kBody,
           kAnnounce, kFooter, kWindowCount };

    void destroy();
    static WINDOW* open(const Rect& r);

    Geometry geometry_;
    WINDOW* windows_[kWindowCount];
};

} // namespace ui
} // namespace tividy

#endif
