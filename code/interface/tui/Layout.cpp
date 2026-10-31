#include "Layout.h"

namespace tividy {
namespace ui {

Layout::Layout() : header_(0), list_(0), detail_(0), body_(0), status_(0), tooSmall_(true) {
    rebuild();
}

Layout::~Layout() {
    destroy();
}

void Layout::rebuild() {
    destroy();
    erase();
    wnoutrefresh(stdscr);

    tooSmall_ = COLS < kMinCols || LINES < kMinRows;
    if (tooSmall_) {
        return;
    }

    const int bodyTop = 1;
    const int bodyHeight = LINES - 3;
    const int listWidth = COLS * 2 / 5;

    header_ = newwin(1, COLS, 0, 0);
    list_ = newwin(bodyHeight, listWidth, bodyTop, 0);
    detail_ = newwin(bodyHeight, COLS - listWidth, bodyTop, listWidth);
    body_ = newwin(bodyHeight, COLS, bodyTop, 0);
    status_ = newwin(2, COLS, LINES - 2, 0);
}

void Layout::destroy() {
    WINDOW** windows[] = {&header_, &list_, &detail_, &body_, &status_};
    for (int i = 0; i < 5; ++i) {
        if (*windows[i] != 0) {
            delwin(*windows[i]);
            *windows[i] = 0;
        }
    }
}

} // namespace ui
} // namespace tividy
