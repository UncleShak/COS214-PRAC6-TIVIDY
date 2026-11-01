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

/** Where the branch goes: its first station, the bend under the drops, and the track ends. */
struct BranchGeometry {
    int start, bend, trackStart, trackEnd;

    BranchGeometry(WINDOW* w, const RouteModel& route, int splitColumn) {
        const int count = static_cast<int>(route.branch.size());
        const int right = getmaxx(w) - 6;
        bend = splitColumn + 3;
        start = splitColumn + 4;
        if (start + (count - 1) * kStationSpacing > right) {
            start = std::max(3, right - (count - 1) * kStationSpacing);
        }
        trackStart = std::min(bend, start);
        trackEnd = start + (count - 1) * kStationSpacing;
    }
    int stationX(int j) const { return start + j * kStationSpacing; }
    /** Path index of column @p x on the branch row: the drops are cells 0 and 1. */
    int cellOf(int x) const { return 2 + std::abs(x - bend); }
};

int splitColumnOf(WINDOW* w, const RouteModel& route) {
    const std::vector<int> columns = stationColumns(static_cast<int>(route.main.size()), getmaxx(w) - 4);
    if (columns.empty() || route.branch.empty() || route.split < 0 ||
        route.split >= static_cast<int>(columns.size())) {
        return -1;
    }
    return columns[route.split];
}

void drawBranch(WINDOW* w, const RouteModel& route, int splitColumn, int branchFill) {
    const int count = static_cast<int>(route.branch.size());
    const BranchGeometry g(w, route, splitColumn);
    const bool taken = route.branchTaken;
    // Cells below `fill` are travelled: thick red track and lit stations. Not animating, the
    // train has travelled as far as the furthest branch stop it reached.
    int travelled = 0;
    for (int j = 0; j < count; ++j) {
        if (reached(route.branch[j])) {
            travelled = std::max(travelled, g.cellOf(g.stationX(j)) + 1);
        }
    }
    const int fill = !taken ? 0 : (branchFill < 0 ? travelled : branchFill);

    // Drawn like the main line: dim track and hollow stops until the train travels it, then red.
    putGlyphs(w, 4, splitColumn + 1, kDrop, 0 < fill ? Tone::Red : Tone::Dim);
    putGlyphs(w, 5, splitColumn + 2, kDrop, 1 < fill ? Tone::Red : Tone::Dim);
    for (int x = g.trackStart; x <= g.trackEnd; ++x) {
        putGlyphs(w, kBranchRow, x, kThick, g.cellOf(x) < fill ? Tone::Red : Tone::Dim);
    }
    for (int j = 0; j < count; ++j) {
        const Station& s = route.branch[j];
        const int x = g.stationX(j);
        const bool lit = reached(s) && g.cellOf(x) < fill;
        const char* glyph = !lit ? kStop : (s.kind == Station::Kind::Current ? kCurrent : kDone);
        const Tone tone = lit ? Tone::Red : (s.kind == Station::Kind::Skipped ? Tone::Dim : Tone::Muted);
        putGlyphs(w, kBranchRow, x, glyph, tone);
        putLabel(w, kBranchLabelRow, x, s.label, j == route.branchCurrent ? Tone::Bright : Tone::Muted);
    }
    // While animating, the train itself: a red ◉ at the head of the fill, down the drops and
    // along the branch towards the stop it is heading for.
    if (taken && branchFill >= 0) {
        int target = route.branchCurrent;
        for (int j = 0; j < count && target < 0; ++j) {
            if (reached(route.branch[j])) {
                target = j;
            }
        }
        const int targetX = g.stationX(target < 0 ? 0 : target);
        if (branchFill <= g.cellOf(targetX)) {
            if (branchFill == 0) {
                putGlyphs(w, 4, splitColumn + 1, kCurrent, Tone::Red);
            } else if (branchFill == 1) {
                putGlyphs(w, 5, splitColumn + 2, kCurrent, Tone::Red);
            } else {
                const int direction = targetX >= g.bend ? 1 : -1;
                putGlyphs(w, kBranchRow, g.bend + direction * (branchFill - 2), kCurrent, Tone::Red);
            }
        }
    }
    if (!taken) {
        const std::string note = "exception route \u00B7 not taken";
        if (g.trackEnd + 3 + static_cast<int>(cells(note)) < getmaxx(w) - 2) {
            putText(w, kBranchRow, g.trackEnd + 3, note, Tone::Muted, 1);
        }
    }
}

} // namespace

int branchCellsTo(void* window, const RouteModel& route, int stop) {
    WINDOW* w = static_cast<WINDOW*>(window);
    const int splitColumn = splitColumnOf(w, route);
    if (splitColumn < 0 || stop < 0 || stop >= static_cast<int>(route.branch.size())) {
        return 0;
    }
    const BranchGeometry g(w, route, splitColumn);
    return g.cellOf(g.stationX(stop)) + 1;
}

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
