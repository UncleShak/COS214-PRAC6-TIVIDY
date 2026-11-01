#include "Layout.h"

#include "Theme.h"

namespace tividy {
namespace ui {

Layout::Layout() {
    for (int i = 0; i < kWindowCount; ++i) {
        windows_[i] = 0;
    }
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
    const Rect rects[kWindowCount] = {geometry_.header, geometry_.route, geometry_.menu, geometry_.list,
                                      geometry_.detail, geometry_.pageMenu, geometry_.pageList,
                                      geometry_.pageDetail, geometry_.body, geometry_.announce,
                                      geometry_.footer};
    for (int i = 0; i < kWindowCount; ++i) {
        windows_[i] = open(rects[i]);
    }
}

void Layout::destroy() {
    for (int i = 0; i < kWindowCount; ++i) {
        if (windows_[i] != 0) {
            delwin(windows_[i]);
            windows_[i] = 0;
        }
    }
}

} // namespace ui
} // namespace tividy
