#include "Layout.h"

#include "Theme.h"

namespace tividy {
namespace ui {

Layout::Layout()
    : title_(0), calendar_(0), menu_(0), list_(0), detail_(0), body_(0), ticker_(0), status_(0) {
    rebuild();
}

Layout::~Layout() {
    destroy();
}

WINDOW* Layout::open(const Rect& r) {
    if (r.empty()) {
        return 0;
    }
    WINDOW* w = newwin(r.h, r.w, r.y, r.x);
    Theme::paint(w);
    return w;
}

void Layout::rebuild() {
    destroy();
    Theme::paint(stdscr);
    erase();
    wnoutrefresh(stdscr);

    geometry_ = Geometry::compute(COLS, LINES);
    if (geometry_.tooSmall) {
        return;
    }
    title_ = open(geometry_.title);
    calendar_ = open(geometry_.calendar);
    menu_ = open(geometry_.menu);
    list_ = open(geometry_.list);
    detail_ = open(geometry_.detail);
    body_ = open(geometry_.body);
    ticker_ = open(geometry_.ticker);
    status_ = open(geometry_.status);
}

void Layout::destroy() {
    WINDOW** windows[] = {&title_, &calendar_, &menu_, &list_, &detail_, &body_, &ticker_, &status_};
    for (int i = 0; i < 8; ++i) {
        if (*windows[i] != 0) {
            delwin(*windows[i]);
            *windows[i] = 0;
        }
    }
}

} // namespace ui
} // namespace tividy
