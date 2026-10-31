/**
 * @file LayoutGeometry.h
 * @brief Where every pane of the ncurses screen goes, as plain numbers (no ncurses).
 *
 * @code
 *   row 0         title band                                   (full width)
 *   row 1         calendar strip: clock, industry, instance, actor, undo
 *   rows 2..H-4   menu | list | detail        or     menu | body
 *   row H-3       event ticker
 *   rows H-2..H-1 key hints, then the last message
 * @endcode
 *
 * The menu column (Persona-style command list) is shown from kMenuMinCols columns; on a
 * narrower terminal it folds away and its keys appear in the hints instead. Kept separate
 * from Layout so the sizes are unit-tested without a terminal (tests/lust/test_layout.cpp).
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_LAYOUT_GEOMETRY_H
#define TIVIDY_LAYOUT_GEOMETRY_H

namespace tividy {
namespace ui {

/** @brief A window's position and size, in terminal cells. Empty when h or w is 0. */
struct Rect {
    int y, x, h, w;
    Rect() : y(0), x(0), h(0), w(0) {}
    Rect(int top, int left, int height, int width) : y(top), x(left), h(height), w(width) {}
    bool empty() const { return h <= 0 || w <= 0; }
};

/** @brief Every pane for one terminal size. */
struct Geometry {
    enum { kMinCols = 80, kMinRows = 24, kMenuMinCols = 110, kMenuWidth = 24 };

    bool tooSmall;
    bool showMenu;
    Rect title, calendar, menu, list, detail, body, ticker, status;

    Geometry() : tooSmall(true), showMenu(false) {}

    /** @brief Lays out a cols x rows terminal. Below the minimum every rect is empty. */
    static Geometry compute(int cols, int rows) {
        Geometry g;
        g.tooSmall = cols < kMinCols || rows < kMinRows;
        if (g.tooSmall) {
            return g;
        }
        g.showMenu = cols >= kMenuMinCols;
        const int contentTop = 2;
        const int contentHeight = rows - 5;
        const int menuWidth = g.showMenu ? kMenuWidth : 0;
        const int paneLeft = menuWidth;
        const int paneWidth = cols - menuWidth;
        const int listWidth = paneWidth * 2 / 5;

        g.title = Rect(0, 0, 1, cols);
        g.calendar = Rect(1, 0, 1, cols);
        if (g.showMenu) {
            g.menu = Rect(contentTop, 0, contentHeight, menuWidth);
        }
        g.list = Rect(contentTop, paneLeft, contentHeight, listWidth);
        g.detail = Rect(contentTop, paneLeft + listWidth, contentHeight, paneWidth - listWidth);
        g.body = Rect(contentTop, paneLeft, contentHeight, paneWidth);
        g.ticker = Rect(rows - 3, 0, 1, cols);
        g.status = Rect(rows - 2, 0, 2, cols);
        return g;
    }
};

} // namespace ui
} // namespace tividy

#endif
