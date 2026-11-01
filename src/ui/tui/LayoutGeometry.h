/**
 * @file LayoutGeometry.h
 * @brief Where every pane of the ncurses screen goes, as plain numbers (no ncurses).
 *
 * @code
 *   rows 0..1      header band                                  (full width)
 *   row 2          blank
 *   dashboard:     rows 3..11  route box                        (full width)
 *                  rows 12..H-4  menu (20) | work items (41+) | detail
 *   other screens: rows 3..H-4   menu (20) | list | detail   or   menu | body
 *   row H-3        announcement bar
 *   row H-2        blank
 *   row H-1        footer (key hints)
 * @endcode
 *
 * At the minimum size, 104 x 34, the dashboard panes are rows 12..30, the announcement bar
 * is row 31 and the footer row 33. Extra height goes to the panes, extra width to the
 * work items and detail. Kept separate from Layout so the sizes are unit-tested without a
 * terminal (tests/ui/test_layout.cpp).
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
    enum { kMinCols = 104, kMinRows = 34, kMenuWidth = 20, kListWidth = 41, kRouteHeight = 9 };

    bool tooSmall;
    Rect header;                       ///< 2-row band
    Rect route, menu, list, detail;    ///< dashboard
    Rect pageMenu, pageList, pageDetail, body; ///< every other screen
    Rect announce, footer;

    Geometry() : tooSmall(true) {}

    /** @brief Width of the work-item / list column for a terminal @p cols wide. */
    static int listWidth(int cols) {
        const int share = (cols - kMenuWidth) * 2 / 5;
        return share > kListWidth ? share : kListWidth;
    }

    /** @brief Lays out a cols x rows terminal. Below the minimum every rect is empty. */
    static Geometry compute(int cols, int rows) {
        Geometry g;
        g.tooSmall = cols < kMinCols || rows < kMinRows;
        if (g.tooSmall) {
            return g;
        }
        const int listW = listWidth(cols);
        const int detailX = kMenuWidth + listW;
        const int detailW = cols - detailX;

        g.header = Rect(0, 0, 2, cols);

        const int routeTop = 3;
        const int paneTop = routeTop + kRouteHeight;   // 12
        const int paneBottom = rows - 4;               // last pane row (30 at 34 rows)
        const int paneH = paneBottom - paneTop + 1;
        g.route = Rect(routeTop, 0, kRouteHeight, cols);
        g.menu = Rect(paneTop, 0, paneH, kMenuWidth);
        g.list = Rect(paneTop, kMenuWidth, paneH, listW);
        g.detail = Rect(paneTop, detailX, paneH, detailW);

        const int pageH = paneBottom - routeTop + 1;
        g.pageMenu = Rect(routeTop, 0, pageH, kMenuWidth);
        g.pageList = Rect(routeTop, kMenuWidth, pageH, listW);
        g.pageDetail = Rect(routeTop, detailX, pageH, detailW);
        g.body = Rect(routeTop, kMenuWidth, pageH, cols - kMenuWidth);

        g.announce = Rect(rows - 3, 0, 1, cols);
        g.footer = Rect(rows - 1, 0, 1, cols);
        return g;
    }
};

} // namespace ui
} // namespace tividy

#endif
