// RouteModel, labels and the compact route: no ncurses (see RouteLine.h; drawing is in RouteLine.cpp).
#include "RouteLine.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace tividy {
namespace ui {

namespace {

const char* const kDone = "●";     // ●
const char* const kCurrent = "◉";  // ◉
const char* const kStop = "○";     // ○
const char* const kThick = "━";    // ━

Station toStation(const WorkItemView& item, const std::string& givenLabel) {
    Station s;
    s.id = item.id;
    s.name = item.name;
    s.label = stationLabel(item.name, givenLabel);
    s.state = item.state;
    s.assignee = item.assigneeName;
    if (item.state == "Completed") {
        s.kind = Station::Kind::Done;
    } else if (item.state == "Rejected") {
        s.kind = Station::Kind::Done;
        s.rejected = true;
    } else if (item.state == "Cancelled") {
        s.kind = Station::Kind::Skipped;
    } else if (item.state == "Created" || item.trackerStatus == "waiting") {
        s.kind = Station::Kind::Upcoming;
    } else {
        s.kind = Station::Kind::Current;
    }
    return s;
}

bool reached(const Station& s) {
    return s.kind == Station::Kind::Done || s.kind == Station::Kind::Current;
}

/** Two stops sharing a first word ("Deploy") are told apart by their second word. */
void separateLabels(std::vector<Station>& stations) {
    for (std::size_t i = 0; i < stations.size(); ++i) {
        for (std::size_t j = i + 1; j < stations.size(); ++j) {
            if (stations[i].label != stations[j].label) {
                continue;
            }
            Station* both[] = {&stations[i], &stations[j]};
            for (int k = 0; k < 2; ++k) {
                const std::string& name = both[k]->name;
                std::size_t first = 1;
                while (first < name.size() && !std::isupper(static_cast<unsigned char>(name[first]))) {
                    ++first;
                }
                if (name.size() > first) {
                    both[k]->label = stationLabel(name.substr(first), "");
                }
            }
        }
    }
}

} // namespace

std::string stationLabel(const std::string& name, const std::string& given) {
    std::string word = given;
    if (word.empty()) {
        std::size_t end = 1;
        while (end < name.size() && !std::isupper(static_cast<unsigned char>(name[end]))) {
            ++end;
        }
        word = name.substr(0, end);
    }
    // A cut word ends in "…" so it does not read as a typo.
    return word.size() > 8 ? word.substr(0, 7) + "\u2026" : word;
}

RouteModel RouteModel::build(const std::vector<WorkItemView>& items, const std::vector<RouteStop>& map) {
    std::map<std::string, const RouteStop*> stops;
    for (std::size_t i = 0; i < map.size(); ++i) {
        stops[map[i].workItemId] = &map[i];
    }

    RouteModel route;
    std::vector<std::string> splitsFrom;   // per branch station
    for (std::size_t i = 0; i < items.size(); ++i) {
        std::map<std::string, const RouteStop*>::const_iterator it = stops.find(items[i].id);
        const RouteStop* stop = it == stops.end() ? 0 : it->second;
        const Station station = toStation(items[i], stop != 0 ? stop->shortLabel : "");
        if (stop != 0 && stop->exception) {
            route.branch.push_back(station);
            splitsFrom.push_back(stop->splitsFrom);
        } else {
            route.main.push_back(station);
        }
    }
    separateLabels(route.main);
    separateLabels(route.branch);

    // A stop not reached while a later one is was passed by (an alternative path).
    int lastReached = -1;
    for (std::size_t i = 0; i < route.main.size(); ++i) {
        if (reached(route.main[i])) {
            lastReached = static_cast<int>(i);
        }
    }
    for (int i = 0; i < lastReached; ++i) {
        if (route.main[i].kind == Station::Kind::Upcoming) {
            route.main[i].kind = Station::Kind::Skipped;
        }
    }

    // The branch: where it splits from, and whether the train took it.
    int splitStop = -1;
    for (std::size_t j = 0; j < route.branch.size(); ++j) {
        if (reached(route.branch[j])) {
            if (!route.branchTaken) {
                splitStop = static_cast<int>(j);
            }
            route.branchTaken = true;
            if (route.branchCurrent < 0 && route.branch[j].kind == Station::Kind::Current) {
                route.branchCurrent = static_cast<int>(j);
            }
        }
    }
    if (!route.branch.empty()) {
        const std::string from = splitsFrom[splitStop < 0 ? 0 : splitStop];
        route.split = 0;
        for (std::size_t i = 0; i < route.main.size(); ++i) {
            if (route.main[i].id == from) {
                route.split = static_cast<int>(i);
            }
        }
    }
    if (route.branchTaken) {
        for (std::size_t i = route.split + 1; i < route.main.size(); ++i) {
            if (route.main[i].kind == Station::Kind::Upcoming) {
                route.main[i].kind = Station::Kind::Skipped;
            }
        }
    }

    for (std::size_t i = 0; i < route.main.size(); ++i) {
        if (route.main[i].kind == Station::Kind::Done) {
            ++route.done;
        }
        if (route.current < 0 && route.main[i].kind == Station::Kind::Current) {
            route.current = static_cast<int>(i);
        }
    }
    route.arrived = route.current < 0 && route.branchCurrent < 0 && lastReached >= 0;
    for (std::size_t i = 0; i < route.main.size() && route.arrived; ++i) {
        if (route.main[i].kind == Station::Kind::Upcoming) {
            route.arrived = false;
        }
    }
    return route;
}

const Station* RouteModel::currentStation() const {
    if (branchCurrent >= 0) {
        return &branch[branchCurrent];
    }
    return current >= 0 ? &main[current] : 0;
}

const Station* RouteModel::nextStation() const {
    if (branchCurrent >= 0) {
        for (std::size_t j = branchCurrent + 1; j < branch.size(); ++j) {
            if (branch[j].kind == Station::Kind::Upcoming) {
                return &branch[j];
            }
        }
        return 0;
    }
    if (current < 0) {
        return 0;
    }
    for (std::size_t i = current + 1; i < main.size(); ++i) {
        if (main[i].kind == Station::Kind::Upcoming) {
            return &main[i];
        }
    }
    return 0;
}

const Station* RouteModel::lastReached() const {
    const Station* now = currentStation();
    if (now != 0) {
        return now;
    }
    if (branchTaken) {
        for (std::size_t j = branch.size(); j-- > 0;) {
            if (reached(branch[j])) {
                return &branch[j];
            }
        }
    }
    for (std::size_t i = main.size(); i-- > 0;) {
        if (reached(main[i])) {
            return &main[i];
        }
    }
    return 0;
}

std::vector<int> stationColumns(int stationCount, int innerWidth) {
    std::vector<int> columns;
    if (stationCount <= 0) {
        return columns;
    }
    const int first = 4;                 // room for half a label left of the first stop
    const int last = innerWidth - 5;
    if (stationCount == 1) {
        columns.push_back(2 + innerWidth / 2);
        return columns;
    }
    const int spacing = (last - first) / (stationCount - 1);
    if (spacing < kStationSpacing) {
        return columns;
    }
    for (int i = 0; i < stationCount; ++i) {
        columns.push_back(2 + first + i * spacing);
    }
    return columns;
}

std::vector<std::pair<std::string, Tone> > miniRoute(const RouteModel& route, int dashes) {
    typedef std::pair<std::string, Tone> Cell;
    std::vector<Cell> out;
    std::string track;
    for (int d = 0; d < dashes; ++d) {
        track += kThick;
    }
    for (std::size_t i = 0; i < route.main.size(); ++i) {
        const Station& s = route.main[i];
        if (i > 0) {
            const bool travelled = reached(s) && !(route.branchTaken && static_cast<int>(i) > route.split);
            out.push_back(Cell(track, travelled ? Tone::Track : Tone::Dim));
        }
        switch (s.kind) {
        case Station::Kind::Done:     out.push_back(Cell(kDone, s.rejected ? Tone::Red : Tone::Bright)); break;
        case Station::Kind::Current:  out.push_back(Cell(kCurrent, Tone::Line)); break;
        case Station::Kind::Upcoming: out.push_back(Cell(kStop, Tone::Muted)); break;
        case Station::Kind::Skipped:  out.push_back(Cell(kStop, Tone::Dim)); break;
        }
        if (route.branchTaken && static_cast<int>(i) == route.split) {
            out.push_back(Cell("╲", Tone::Red));   // ╲
            out.push_back(Cell(route.branchCurrent >= 0 ? kCurrent : kDone, Tone::Red));
        }
    }
    return out;
}

} // namespace ui
} // namespace tividy
