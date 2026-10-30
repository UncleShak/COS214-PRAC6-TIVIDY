#include "TuiFrontEnd.h"

#include <cstdio>

#include "Log.h"

namespace tividy {
namespace ui {

TuiFrontEnd::TuiFrontEnd(InterfaceController& controller, const std::string& engineLabel)
    : controller_(controller), engineLabel_(engineLabel), selected_(0), running_(true) {}

void TuiFrontEnd::run() {
    draw();
    while (running_) {
        int key = getch();
        const int count = static_cast<int>(controller_.view().industries.size());

        switch (key) {
        case KEY_RESIZE:
            Log::write("resize");
            layout_.rebuild();
            break;
        case 'q':
        case 'Q':
            running_ = false;
            break;
        case KEY_UP:
        case 'k':
            if (selected_ > 0) {
                --selected_;
            }
            break;
        case KEY_DOWN:
        case 'j':
            if (selected_ + 1 < count) {
                ++selected_;
            }
            break;
        default:
            break;
        }
        if (running_) {
            draw();
        }
    }
}

void TuiFrontEnd::draw() {
    if (layout_.tooSmall()) {
        drawTooSmall();
        return;
    }
    drawHeader();
    drawList();
    drawDetail();
    drawStatus();
    doupdate();
}

void TuiFrontEnd::drawTooSmall() {
    erase();
    char size[96];
    std::snprintf(size, sizeof(size), "Terminal is %dx%d. TIVIDY needs at least %dx%d.", COLS, LINES,
                  Layout::kMinCols, Layout::kMinRows);
    attron(Theme::attr(Tone::Red));
    mvaddnstr(1, 1, size, COLS - 2);
    attroff(Theme::attr(Tone::Red));
    mvaddnstr(3, 1, "Enlarge the window, or run:  tividy --plain", COLS - 2);
    mvaddnstr(5, 1, "q quits", COLS - 2);
    wnoutrefresh(stdscr);
    doupdate();
}

void TuiFrontEnd::drawHeader() {
    WINDOW* w = layout_.header();
    werase(w);
    put(w, 0, 1, "TIVIDY", Tone::Gold, 0);
    put(w, 0, 9, "Se7en workflow engine", Tone::Bone, 0);
    const std::string label = "[" + engineLabel_ + "]";
    int x = COLS - static_cast<int>(label.size()) - 1;
    put(w, 0, x > 32 ? x : 32, label, Tone::Muted, 0);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawList() {
    WINDOW* w = layout_.list();
    werase(w);
    frame(w, "INDUSTRY", true);

    const std::vector<IndustryInfo>& industries = controller_.view().industries;
    if (industries.empty()) {
        put(w, 2, 2, "No industries registered with the engine.", Tone::Red);
    }
    const int rows = getmaxy(w) - 2;
    for (int i = 0; i < static_cast<int>(industries.size()) && i < rows; ++i) {
        const bool chosen = i == selected_;
        const std::string marker = chosen ? "> " : "  ";
        if (chosen) {
            wattron(w, A_REVERSE);
        }
        put(w, 1 + i, 2, marker + industries[i].displayName, chosen ? Tone::Gold : Tone::Bone);
        if (chosen) {
            wattroff(w, A_REVERSE);
        }
    }
    wnoutrefresh(w);
}

void TuiFrontEnd::drawDetail() {
    WINDOW* w = layout_.detail();
    werase(w);
    frame(w, "ABOUT", false);

    const std::vector<IndustryInfo>& industries = controller_.view().industries;
    if (selected_ < static_cast<int>(industries.size())) {
        const IndustryInfo& industry = industries[selected_];
        put(w, 1, 2, industry.displayName, Tone::Gold);
        put(w, 2, 2, industry.description, Tone::Bone);
        put(w, 4, 2, "Definitions:", Tone::Muted);
        for (std::size_t i = 0; i < industry.definitions.size(); ++i) {
            put(w, 5 + static_cast<int>(i), 4, industry.definitions[i], Tone::Bone);
        }
    }
    wnoutrefresh(w);
}

void TuiFrontEnd::drawStatus() {
    WINDOW* w = layout_.status();
    werase(w);
    put(w, 0, 1, "Up/Down move   q quit        (skeleton: more screens coming)", Tone::Muted, 0);

    StatusMessage status = controller_.status();
    Tone tone = Tone::Bone;
    if (status.kind == StatusMessage::Kind::Error) {
        tone = Tone::Red;
    } else if (status.kind == StatusMessage::Kind::Success) {
        tone = Tone::Gold;
    }
    put(w, 1, 1, status.text, tone, 0);
    wnoutrefresh(w);
}

void TuiFrontEnd::frame(WINDOW* window, const std::string& title, bool focused) {
    int border = Theme::attr(focused ? Tone::Gold : Tone::Muted);
    wattron(window, border);
    box(window, 0, 0);
    wattroff(window, border);
    put(window, 0, 2, " " + title + " ", focused ? Tone::Gold : Tone::Bone);
}

void TuiFrontEnd::put(WINDOW* window, int y, int x, const std::string& text, Tone tone, int margin) {
    int width = getmaxx(window) - x - margin;
    if (width <= 0 || y < 0 || y >= getmaxy(window)) {
        return;
    }
    int attr = Theme::attr(tone);
    wattron(window, attr);
    mvwaddnstr(window, y, x, text.c_str(), width);
    wattroff(window, attr);
}

} // namespace ui
} // namespace tividy
