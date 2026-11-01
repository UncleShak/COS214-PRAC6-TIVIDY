// Drawing the route line in the route box (see RouteLine.h; the model is in RouteModel.cpp).
#include "RouteLine.h"

#include <algorithm>
#include <cstdlib>

#include <curses.h>

#include "Widgets.h"

namespace tividy {
namespace ui {

namespace {

const int kMainRow = 2;
const int kLabelRow = 3;
const int kBranchRow = 6;
const int kBranchLabelRow = 7;

const char* const kDone = "●";
const char* const kCurrent = "◉";
const char* const kStop = "○";
const char* const kThick = "━";
const char* const kThin = "─";
const char* const kDrop = "╲";

bool reached(const Station& s) {
    return s.kind == Station::Kind::Done || s.kind == Station::Kind::Current;
}

void putLabel(WINDOW* w, int y, int column, const std::string& label, Tone tone) {
    const int width = static_cast<int>(cells(label));
    const int x = std::max(2, std::min(column - width / 2, getmaxx(w) - 2 - width));
    putText(w, y, x, label, tone, 1);
}

void putStation(WINDOW* w, int y, int x, const Station& s, bool asUpcoming) {
    if (asUpcoming) {
        putGlyphs(w, y, x, kStop, Tone::Muted);
        return;
    }
    switch (s.kind) {
    case Station::Kind::Done:     putGlyphs(w, y, x, kDone, s.rejected ? Tone::Red : Tone::Bright); break;
    case Station::Kind::Current:  putGlyphs(w, y, x, kCurrent, Tone::Line); break;
    case Station::Kind::Upcoming: putGlyphs(w, y, x, kStop, Tone::Muted); break;
    case Station::Kind::Skipped:  putGlyphs(w, y, x, kStop, Tone::Dim); break;
    }
}

/** The narrow form: ●━●━◉━○━○ and only the current stop's name. */
void drawCompact(WINDOW* w, const RouteModel& route) {
    const std::vector<std::pair<std::string, Tone> > cells = miniRoute(route, 1);
    Line line;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        line.add(cells[i].first, cells[i].second);
    }
    putLine(w, kMainRow, 2, getmaxx(w) - 4, line, false);
    const Station* now = route.lastReached();
    if (now != 0) {
        putText(w, kLabelRow + 1, 2, now->name, Tone::Bright, 2);
    }
}

void drawBranch(WINDOW* w, const RouteModel& route, int splitColumn, int branchFill) {
    const int count = static_cast<int>(route.branch.size());
    const int right = getmaxx(w) - 6;
    int start = splitColumn + 4;
    if (start + (count - 1) * kStationSpacing > right) {
        start = std::max(3, right - (count - 1) * kStationSpacing);
    }
    const int trackStart = std::min(splitColumn + 3, start);
    const int trackEnd = start + (count - 1) * kStationSpacing;
    // Path order for the fill animation: the two drops, then the track left to right.
    const bool taken = route.branchTaken;
    struct Fill {
        int limit;
        bool operator()(int cell) const { return limit < 0 || cell < limit; }
    } filled = {taken ? branchFill : 0};

    putGlyphs(w, 4, splitColumn + 1, kDrop, Tone::Red);
    putGlyphs(w, 5, splitColumn + 2, kDrop, Tone::Red);
    for (int x = trackStart; x <= trackEnd; ++x) {
        const int cell = 2 + std::abs(x - (splitColumn + 3));
        putGlyphs(w, kBranchRow, x, filled(cell) ? kThick : kThin, Tone::Red);
    }
    for (int j = 0; j < count; ++j) {
        const Station& s = route.branch[j];
        const int x = start + j * kStationSpacing;
        const bool on = reached(s) && filled(2 + std::abs(x - (splitColumn + 3)));
        const char* glyph = !on ? kStop : (s.kind == Station::Kind::Current ? kCurrent : kDone);
        putGlyphs(w, kBranchRow, x, glyph, Tone::Red);
        putLabel(w, kBranchLabelRow, x, s.label, j == route.branchCurrent ? Tone::Bright : Tone::Muted);
    }
    if (!taken) {
        const std::string note = "exception route · not taken";
        if (trackEnd + 3 + static_cast<int>(cells(note)) < getmaxx(w) - 2) {
            putText(w, kBranchRow, trackEnd + 3, note, Tone::Muted, 1);
        }
    }
}

} // namespace

void drawRoute(void* window, const RouteModel& route, int travelTo, int branchFill) {
    WINDOW* w = static_cast<WINDOW*>(window);
    const int inner = getmaxx(w) - 4;
    const int count = static_cast<int>(route.main.size());
    if (count == 0) {
        putText(w, kMainRow, 2, "No stops on this route.", Tone::Muted, 2);
        return;
    }
    const std::vector<int> columns = stationColumns(count, inner);
    if (columns.empty()) {
        drawCompact(w, route);
        return;
    }

    for (int i = 0; i + 1 < count; ++i) {
        const bool travelled = reached(route.main[i + 1]) && !(route.branchTaken && i >= route.split);
        for (int x = columns[i] + 1; x < columns[i + 1]; ++x) {
            const bool on = travelled && (travelTo < 0 || x <= travelTo);
            putGlyphs(w, kMainRow, x, kThick, on ? Tone::Track : Tone::Dim);
        }
    }
    for (int i = 0; i < count; ++i) {
        const Station& s = route.main[i];
        const bool ahead = travelTo >= 0 && columns[i] > travelTo && s.kind == Station::Kind::Current;
        putStation(w, kMainRow, columns[i], s, ahead);
        Tone label = s.kind == Station::Kind::Skipped ? Tone::Dim : Tone::Muted;
        if (i == route.current && route.branchCurrent < 0) {
            label = Tone::Bright;
        }
        putLabel(w, kLabelRow, columns[i], s.label, label);
    }
    if (travelTo >= 0) {
        putGlyphs(w, kMainRow, travelTo, kCurrent, Tone::Line);
    }
    if (!route.branch.empty() && route.split >= 0) {
        drawBranch(w, route, columns[route.split], branchFill);
    }
}

} // namespace ui
} // namespace tividy
