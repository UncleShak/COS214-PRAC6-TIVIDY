// Screen geometry of the ncurses front end (no terminal needed).
#include "doctest.h"

#include "tui/LayoutGeometry.h"

using tividy::ui::Geometry;
using tividy::ui::Rect;

namespace {

bool inside(const Rect& r, int cols, int rows) {
    return r.y >= 0 && r.x >= 0 && r.y + r.h <= rows && r.x + r.w <= cols;
}

bool overlaps(const Rect& a, const Rect& b) {
    if (a.empty() || b.empty()) return false;
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// Every visible pane fits on screen, and panes drawn together never overlap.
void checkConsistent(const Geometry& g, int cols, int rows) {
    const Rect all[] = {g.title, g.calendar, g.menu, g.list, g.detail, g.body, g.ticker, g.status};
    for (int i = 0; i < 8; ++i) {
        if (!all[i].empty()) CHECK(inside(all[i], cols, rows));
    }
    const Rect withPanes[] = {g.title, g.calendar, g.menu, g.list, g.detail, g.ticker, g.status};
    for (int i = 0; i < 7; ++i)
        for (int j = i + 1; j < 7; ++j) CHECK_FALSE(overlaps(withPanes[i], withPanes[j]));
    const Rect withBody[] = {g.title, g.calendar, g.menu, g.body, g.ticker, g.status};
    for (int i = 0; i < 6; ++i)
        for (int j = i + 1; j < 6; ++j) CHECK_FALSE(overlaps(withBody[i], withBody[j]));
    // list + detail cover exactly the body
    CHECK(g.list.x == g.body.x);
    CHECK(g.list.w + g.detail.w == g.body.w);
    CHECK(g.list.h == g.body.h);
}

}  // namespace

TEST_SUITE("lust layout") {

TEST_CASE("80x24: the smallest supported terminal folds the menu away") {
    const Geometry g = Geometry::compute(80, 24);
    REQUIRE_FALSE(g.tooSmall);
    CHECK_FALSE(g.showMenu);
    CHECK(g.menu.empty());
    CHECK(g.body.x == 0);
    CHECK(g.body.w == 80);
    CHECK(g.body.h == 19);
    CHECK(g.ticker.y == 21);
    CHECK(g.status.y == 22);
    checkConsistent(g, 80, 24);
}

TEST_CASE("200x60: a large terminal shows the menu column") {
    const Geometry g = Geometry::compute(200, 60);
    REQUIRE_FALSE(g.tooSmall);
    CHECK(g.showMenu);
    CHECK(g.menu.w == Geometry::kMenuWidth);
    CHECK(g.body.x == Geometry::kMenuWidth);
    CHECK(g.list.w == (200 - Geometry::kMenuWidth) * 2 / 5);
    CHECK(g.status.y + g.status.h == 60);
    checkConsistent(g, 200, 60);
}

TEST_CASE("below 80x24 the screen is too small and nothing is laid out") {
    CHECK(Geometry::compute(79, 24).tooSmall);
    CHECK(Geometry::compute(80, 23).tooSmall);
    const Geometry g = Geometry::compute(40, 10);
    CHECK(g.body.empty());
    CHECK(g.status.empty());
}

TEST_CASE("resizing: every size from the minimum up is consistent, and the menu appears at 110") {
    for (int cols = 80; cols <= 220; cols += 7) {
        for (int rows = 24; rows <= 70; rows += 5) {
            const Geometry g = Geometry::compute(cols, rows);
            REQUIRE_FALSE(g.tooSmall);
            CHECK(g.showMenu == (cols >= Geometry::kMenuMinCols));
            checkConsistent(g, cols, rows);
        }
    }
    CHECK_FALSE(Geometry::compute(109, 30).showMenu);
    CHECK(Geometry::compute(110, 30).showMenu);
}

}  // TEST_SUITE
