#include "Widgets.h"

#include <algorithm>
#include <cstdio>

namespace tividy {
namespace ui {

namespace {

/** Centred window for a dialog, never larger than the terminal. */
WINDOW* openModal(int height, int width) {
    height = std::min(height, LINES - 2);
    width = std::min(width, COLS - 4);
    WINDOW* window = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2);
    keypad(window, TRUE);
    Theme::paint(window);
    return window;
}

/** Text that fits in @p width, keeping the end (where the user is typing). */
std::string tail(const std::string& text, std::size_t width) {
    return text.size() > width ? text.substr(text.size() - width) : text;
}

bool isPrintable(int key) {
    return key >= 32 && key <= 126;
}

/** Keeps @p top so that @p selected is one of the @p visible rows starting at it. */
int scrollTo(int selected, int top, int visible) {
    if (selected < top) {
        return selected;
    }
    if (selected >= top + visible) {
        return selected - visible + 1;
    }
    return top;
}

} // namespace

std::size_t cells(const std::string& text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

std::string cutCells(const std::string& text, std::size_t width) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            if (count == width) {
                return text.substr(0, i);
            }
            ++count;
        }
    }
    return text;
}

void putText(WINDOW* window, int y, int x, const std::string& text, Tone tone, int margin) {
    int width = getmaxx(window) - x - margin;
    if (width <= 0 || y < 0 || y >= getmaxy(window)) {
        return;
    }
    const std::string shown = cutCells(text, static_cast<std::size_t>(width));
    int attr = Theme::attr(tone);
    wattron(window, attr);
    mvwaddstr(window, y, x, shown.c_str());
    wattroff(window, attr);
}

void putGlyphs(WINDOW* window, int y, int x, const char* glyphs, Tone tone) {
    if (y < 0 || y >= getmaxy(window) || x < 0 || x >= getmaxx(window)) {
        return;
    }
    const int attr = Theme::attr(tone);
    wattron(window, attr);
    mvwaddstr(window, y, x, glyphs);
    wattroff(window, attr);
}

namespace {

#ifdef NCURSES_WIDECHAR
/** One wide box-drawing character as a cchar_t, for wborder_set. */
cchar_t boxChar(const wchar_t* glyph) {
    cchar_t c;
    setcchar(&c, glyph, A_NORMAL, 0, 0);
    return c;
}
#endif

} // namespace

void drawFrame(WINDOW* window, const std::string& title, bool focused, const std::string& right) {
    const int border = Theme::attr(focused ? Tone::BorderFocus : Tone::Border);
    wattron(window, border);
#ifdef NCURSES_WIDECHAR
    const cchar_t vertical = boxChar(L"\u2502");
    const cchar_t horizontal = boxChar(L"\u2500");
    const cchar_t topLeft = boxChar(L"\u256D");
    const cchar_t topRight = boxChar(L"\u256E");
    const cchar_t bottomLeft = boxChar(L"\u2570");
    const cchar_t bottomRight = boxChar(L"\u256F");
    wborder_set(window, &vertical, &vertical, &horizontal, &horizontal, &topLeft, &topRight, &bottomLeft,
                &bottomRight);
#else
    box(window, 0, 0);
#endif
    wattroff(window, border);

    const int width = getmaxx(window);
    int plateEnd = 2;
    if (!title.empty() && width > 8) {
        const std::string plate = " " + title + " ";
        const std::size_t room = static_cast<std::size_t>(width - 4);
        const std::string shown = plate.substr(0, std::min(plate.size(), room));
        putText(window, 0, 2, shown, focused ? Tone::Plate : Tone::PlateBand, 0);
        plateEnd = 2 + static_cast<int>(shown.size());
    }
    if (!right.empty()) {
        const std::string summary = " " + right + " ";
        const int x = width - 2 - static_cast<int>(summary.size());
        if (x > plateEnd + 1) {
            putText(window, 0, x, summary, Tone::Muted, 0);
        }
    }
}

void fillRow(WINDOW* window, int y, int x, int width, Tone tone) {
    if (width > 0) {
        putText(window, y, x, std::string(static_cast<std::size_t>(width), ' '), tone, 0);
    }
}

void putRow(WINDOW* window, int y, int x, int width, const std::string& text, Tone tone, bool chosen) {
    if (width <= 2) {
        return;
    }
    if (chosen) {
        putText(window, y, x, fit("  " + text, static_cast<std::size_t>(width)), Tone::Select, 0);
        putGlyphs(window, y, x, "\u25B6", Tone::Select);
    } else {
        putText(window, y, x, fit("  " + text, static_cast<std::size_t>(width)), tone, 0);
    }
}

std::string fit(const std::string& text, std::size_t width) {
    const std::size_t used = cells(text);
    if (used >= width) {
        return cutCells(text, width);
    }
    return text + std::string(width - used, ' ');
}

bool isEnter(int key) {
    return key == '\n' || key == '\r' || key == KEY_ENTER;
}

bool isBackspace(int key) {
    return key == KEY_BACKSPACE || key == 127 || key == 8;
}

int Dialog::menu(const std::string& title, const std::vector<Line>& entries, int initial) {
    if (entries.empty()) {
        return kCancelled;
    }
    std::size_t widest = title.size() + 6;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        widest = std::max(widest, entries[i].text.size() + 6);
    }
    const int width = std::max(34, static_cast<int>(widest));
    const int count = static_cast<int>(entries.size());
    const int visible = std::max(1, std::min(count, LINES - 8));
    WINDOW* w = openModal(visible + 3, width);

    int selected = std::max(0, std::min(initial, count - 1));
    int top = 0;
    int result = kCancelled;
    for (;;) {
        top = scrollTo(selected, top, visible);
        werase(w);
        drawFrame(w, title, true);
        const int inner = getmaxx(w) - 4;
        for (int row = 0; row < visible && top + row < count; ++row) {
            const int index = top + row;
            const bool chosen = index == selected;
            putRow(w, 1 + row, 2, inner, entries[index].text, entries[index].tone, chosen);
        }
        putText(w, getmaxy(w) - 2, 2, count > visible ? "Enter choose   Esc close   (more: scroll)"
                                                      : "Enter choose   Esc close", Tone::Muted, 2);

        const int key = wgetch(w);
        if (key == KEY_RESIZE) {
            ungetch(KEY_RESIZE);
            break;
        }
        if (key == kKeyEscape || key == 'q') {
            break;
        }
        if (isEnter(key)) {
            result = selected;
            break;
        }
        if ((key == KEY_UP || key == 'k') && selected > 0) {
            --selected;
        } else if ((key == KEY_DOWN || key == 'j') && selected + 1 < count) {
            ++selected;
        } else if (key == KEY_PPAGE) {
            selected = std::max(0, selected - visible);
        } else if (key == KEY_NPAGE) {
            selected = std::min(count - 1, selected + visible);
        } else if (key == KEY_HOME) {
            selected = 0;
        } else if (key == KEY_END) {
            selected = count - 1;
        }
    }
    delwin(w);
    return result;
}

bool Dialog::prompt(const std::string& title, const std::string& label, std::string& value,
                    std::size_t maxLength) {
    const int width = std::max(64, static_cast<int>(label.size()) + 8);
    WINDOW* w = openModal(7, width);
    std::string typed = value;
    bool accepted = false;

    curs_set(1);
    for (;;) {
        werase(w);
        drawFrame(w, title, true);
        putText(w, 1, 2, label, Tone::Text, 2);
        const std::size_t room = static_cast<std::size_t>(std::max(1, getmaxx(w) - 8));
        const std::string shown = tail(typed, room);
        putText(w, 3, 2, "> ", Tone::Line, 2);
        putText(w, 3, 4, shown, Tone::Text, 2);
        putText(w, 5, 2, "Enter confirm   Esc cancel", Tone::Muted, 2);
        wmove(w, 3, 4 + static_cast<int>(shown.size()));

        const int key = wgetch(w);
        if (key == KEY_RESIZE) {
            ungetch(KEY_RESIZE);
            break;
        }
        if (key == kKeyEscape) {
            break;
        }
        if (isEnter(key)) {
            value = typed;
            accepted = true;
            break;
        }
        if (isBackspace(key)) {
            if (!typed.empty()) {
                typed.erase(typed.size() - 1);
            }
        } else if (isPrintable(key) && typed.size() < maxLength) {
            typed += static_cast<char>(key);
        }
    }
    curs_set(0);
    delwin(w);
    return accepted;
}

bool Dialog::confirm(const std::string& question) {
    WINDOW* w = openModal(5, std::max(44, static_cast<int>(question.size()) + 6));
    bool yes = false;
    for (;;) {
        werase(w);
        drawFrame(w, "CONFIRM", true);
        putText(w, 1, 2, question, Tone::Text, 2);
        putText(w, 3, 2, "Enter/y yes   Esc/n no", Tone::Muted, 2);

        const int key = wgetch(w);
        if (key == KEY_RESIZE) {
            ungetch(KEY_RESIZE);
            break;
        }
        if (isEnter(key) || key == 'y' || key == 'Y') {
            yes = true;
            break;
        }
        if (key == kKeyEscape || key == 'n' || key == 'N') {
            break;
        }
    }
    delwin(w);
    return yes;
}

void Dialog::text(const std::string& title, const std::vector<Line>& lines) {
    WINDOW* w = openModal(LINES - 4, COLS - 8);
    const int count = static_cast<int>(lines.size());
    int top = 0;
    for (;;) {
        const int visible = std::max(1, getmaxy(w) - 3);
        const int last = std::max(0, count - visible);
        top = std::max(0, std::min(top, last));

        werase(w);
        drawFrame(w, title, true);
        if (lines.empty()) {
            putText(w, 1, 2, "(nothing to show)", Tone::Muted, 2);
        }
        for (int row = 0; row < visible && top + row < count; ++row) {
            putText(w, 1 + row, 2, lines[top + row].text, lines[top + row].tone, 2);
        }
        char where[64] = "";
        if (count > visible) {
            std::snprintf(where, sizeof(where), "   lines %d-%d of %d", top + 1,
                          std::min(count, top + visible), count);
        }
        putText(w, getmaxy(w) - 2, 2, std::string("Up/Down scroll   Esc close") + where, Tone::Muted, 2);

        const int key = wgetch(w);
        if (key == KEY_RESIZE) {
            ungetch(KEY_RESIZE);
            break;
        }
        if (key == kKeyEscape || isEnter(key) || key == 'q' || key == 'b') {
            break;
        }
        if (key == KEY_UP || key == 'k') {
            --top;
        } else if (key == KEY_DOWN || key == 'j') {
            ++top;
        } else if (key == KEY_PPAGE) {
            top -= visible;
        } else if (key == KEY_NPAGE) {
            top += visible;
        } else if (key == KEY_HOME) {
            top = 0;
        } else if (key == KEY_END) {
            top = last;
        }
    }
    delwin(w);
}

void Dialog::alert(const std::string& title, const std::string& subtitle, const std::vector<Line>& lines) {
    WINDOW* w = newwin(LINES, COLS, 0, 0);
    keypad(w, TRUE);
    Theme::paint(w);
    werase(w);

    // Diagonal stripes above and below a red band, the title spaced out in the middle.
    std::string stripes;
    for (int i = 0; i < COLS; ++i) {
        stripes += (i % 4 < 2) ? "\u2571" : " ";
    }
    const int band = std::max(1, LINES / 2 - 4);
    putGlyphs(w, band - 2, 0, stripes.c_str(), Tone::Red);
    for (int row = band; row < band + 5; ++row) {
        putText(w, row, 0, std::string(static_cast<std::size_t>(COLS), ' '), Tone::BulletRed, 0);
    }
    std::string spaced;
    for (std::size_t i = 0; i < title.size(); ++i) {
        spaced += (title[i] == '_' ? ' ' : title[i]);
        spaced += ' ';
    }
    const std::string heading = "!  " + spaced + " !";
    putText(w, band + 1, std::max(0, (COLS - static_cast<int>(heading.size())) / 2), heading, Tone::BulletRed, 0);
    putText(w, band + 3, std::max(0, (COLS - static_cast<int>(subtitle.size())) / 2), subtitle, Tone::BulletRed, 0);
    putGlyphs(w, band + 6, 0, stripes.c_str(), Tone::Red);

    int y = band + 8;
    for (std::size_t i = 0; i < lines.size() && y < LINES - 2; ++i, ++y) {
        putText(w, y, std::max(2, COLS / 2 - 24), lines[i].text, lines[i].tone, 2);
    }
    putText(w, LINES - 1, 2, "Press any key", Tone::Muted, 2);
    wrefresh(w);

    const int key = wgetch(w);
    if (key == KEY_RESIZE) {
        ungetch(KEY_RESIZE);
    }
    delwin(w);
    touchwin(stdscr);
}

bool Dialog::form(const std::string& title, const std::vector<DataField>& fields,
                  std::map<std::string, std::string>& values) {
    const int count = static_cast<int>(fields.size());
    if (count == 0) {
        values.clear();
        return true;
    }

    std::vector<std::string> labels;
    std::vector<std::string> typed;
    std::size_t labelWidth = 0;
    for (int i = 0; i < count; ++i) {
        std::string label = fields[i].label;
        if (fields[i].numeric) {
            label += " (number)";
        } else if (fields[i].yesNo) {
            label += " (yes/no)";
        }
        if (fields[i].required) {
            label += " *";
        }
        labels.push_back(label);
        labelWidth = std::max(labelWidth, label.size());
        std::map<std::string, std::string>::const_iterator before = values.find(fields[i].key);
        typed.push_back(before == values.end() ? "" : before->second);
    }

    const int width = std::max(70, static_cast<int>(std::max(labelWidth + 40, title.size() + 8)));
    WINDOW* w = openModal(2 * count + 5, width);
    const int inputX = static_cast<int>(labelWidth) + 4;
    int current = 0;
    std::string error;
    bool accepted = false;

    curs_set(1);
    for (;;) {
        const std::size_t room = static_cast<std::size_t>(std::max(4, getmaxx(w) - inputX - 4));
        werase(w);
        drawFrame(w, title, true);
        for (int i = 0; i < count; ++i) {
            const int y = 2 + 2 * i;
            const bool focused = i == current;
            putText(w, y, 2, labels[i], focused ? Tone::Line : Tone::Text, 2);
            std::string inside = tail(typed[i], room);
            Tone tone = Tone::Text;
            if (inside.empty() && !focused && !fields[i].defaultValue.empty()) {
                inside = "default: " + fields[i].defaultValue;
                tone = Tone::Muted;
            }
            putText(w, y, inputX, "[", focused ? Tone::Line : Tone::Muted, 1);
            putText(w, y, inputX + 1, fit(inside, room), tone, 1);
            putText(w, y, inputX + 1 + static_cast<int>(room), "]", focused ? Tone::Line : Tone::Muted, 1);
        }
        if (!error.empty()) {
            putText(w, 2 * count + 2, 2, "! " + error, Tone::Red, 2);
        }
        putText(w, getmaxy(w) - 2, 2, "Enter next field / start   Up/Down move   Esc cancel   * required",
                Tone::Muted, 2);
        wmove(w, 2 + 2 * current, inputX + 1 + static_cast<int>(tail(typed[current], room).size()));

        const int key = wgetch(w);
        if (key == KEY_RESIZE) {
            ungetch(KEY_RESIZE);
            break;
        }
        if (key == kKeyEscape) {
            break;
        }
        if (key == KEY_UP || key == KEY_BTAB) {
            current = std::max(0, current - 1);
        } else if (key == KEY_DOWN || key == '\t') {
            current = std::min(count - 1, current + 1);
        } else if (isEnter(key)) {
            std::string cleaned;
            error = validateField(fields[current], typed[current], cleaned);
            if (!error.empty()) {
                continue;
            }
            if (current + 1 < count) {
                ++current;
                continue;
            }
            for (int i = 0; i < count && error.empty(); ++i) {
                error = validateField(fields[i], typed[i], cleaned);
                if (!error.empty()) {
                    current = i;
                }
            }
            if (error.empty()) {
                values.clear();
                for (int i = 0; i < count; ++i) {
                    values[fields[i].key] = typed[i];
                }
                accepted = true;
                break;
            }
        } else if (isBackspace(key)) {
            if (!typed[current].empty()) {
                typed[current].erase(typed[current].size() - 1);
            }
            error.clear();
        } else if (isPrintable(key) && typed[current].size() < kMaxTextLength) {
            typed[current] += static_cast<char>(key);
            error.clear();
        }
    }
    curs_set(0);
    delwin(w);
    return accepted;
}

} // namespace ui
} // namespace tividy
