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

void checkNoOverlap(const Rect* rects, int count) {
    for (int i = 0; i < count; ++i)
        for (int j = i + 1; j < count; ++j) CHECK_FALSE(overlaps(rects[i], rects[j]));
}

// Every pane fits on screen, and panes drawn together never overlap.
void checkConsistent(const Geometry& g, int cols, int rows) {
    const Rect all[] = {g.header, g.route, g.menu, g.list, g.detail, g.pageMenu, g.pageList,
                        g.pageDetail, g.body, g.announce, g.footer};
    for (int i = 0; i < 11; ++i) {
        CHECK_FALSE(all[i].empty());
        CHECK(inside(all[i], cols, rows));
    }
    const Rect dashboard[] = {g.header, g.route, g.menu, g.list, g.detail, g.announce, g.footer};
    checkNoOverlap(dashboard, 7);
    const Rect page[] = {g.header, g.pageMenu, g.pageList, g.pageDetail, g.announce, g.footer};
    checkNoOverlap(page, 6);
    const Rect withBody[] = {g.header, g.pageMenu, g.body, g.announce, g.footer};
    checkNoOverlap(withBody, 5);
    // the three columns cover the full width, and the dashboard covers the same rows as a page
    CHECK(g.menu.w + g.list.w + g.detail.w == cols);
    CHECK(g.pageList.w + g.pageDetail.w == g.body.w);
    CHECK(g.route.y == g.pageMenu.y);
    CHECK(g.menu.y + g.menu.h == g.pageMenu.y + g.pageMenu.h);
}

}  // namespace

TEST_SUITE("lust layout") {

TEST_CASE("104x34: the minimum terminal matches the subway dashboard plan") {
    const Geometry g = Geometry::compute(104, 34);
    REQUIRE_FALSE(g.tooSmall);
    CHECK(g.header.y == 0);
    CHECK(g.header.h == 2);
    CHECK(g.route.y == 3);
    CHECK(g.route.y + g.route.h - 1 == 11);
    CHECK(g.menu.y == 12);
    CHECK(g.menu.y + g.menu.h - 1 == 30);
    CHECK(g.menu.w == 20);
    CHECK(g.list.w == 41);
    CHECK(g.detail.w == 104 - 61);
    CHECK(g.announce.y == 31);
    CHECK(g.footer.y == 33);
    checkConsistent(g, 104, 34);
}

TEST_CASE("200x60: extra room goes to the panes") {
    const Geometry g = Geometry::compute(200, 60);
    REQUIRE_FALSE(g.tooSmall);
    CHECK(g.menu.w == Geometry::kMenuWidth);
    CHECK(g.list.w == (200 - Geometry::kMenuWidth) * 2 / 5);
    CHECK(g.route.h == Geometry::kRouteHeight);
    CHECK(g.footer.y == 59);
    checkConsistent(g, 200, 60);
}

TEST_CASE("below 104x34 the screen is too small and nothing is laid out") {
    CHECK(Geometry::compute(103, 34).tooSmall);
    CHECK(Geometry::compute(104, 33).tooSmall);
    const Geometry g = Geometry::compute(80, 24);
    CHECK(g.route.empty());
    CHECK(g.footer.empty());
}

TEST_CASE("resizing: every size from the minimum up is consistent") {
    for (int cols = 104; cols <= 240; cols += 7) {
        for (int rows = 34; rows <= 80; rows += 5) {
            const Geometry g = Geometry::compute(cols, rows);
            REQUIRE_FALSE(g.tooSmall);
            checkConsistent(g, cols, rows);
        }
    }
}

}  // TEST_SUITE
