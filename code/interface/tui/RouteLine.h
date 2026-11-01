/**
 * @file RouteLine.h
 * @brief The subway route line: an instance's work items as stations on a main line, with
 *        the exception route as a red branch dropping from the stop where it splits.
 *
 * RouteModel is built from plain view data (work items and the engine's route map) and has
 * no ncurses, so it is unit-tested in tests/lust/test_route_line.cpp. drawRoute() and
 * miniRoute() turn it into cells. Nothing here knows any industry: what is main line and
 * what is exception comes from EngineGateway::routeMap(). With no route map, every item is
 * a main-line stop.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_ROUTE_LINE_H
#define TIVIDY_ROUTE_LINE_H

#include <string>
#include <vector>

#include "EngineGateway.h"
#include "Theme.h"

namespace tividy {
namespace ui {

/** @brief One stop on the route. */
struct Station {
    /** @brief How far the train has got with this stop. */
    enum class Kind {
        Done,     ///< Completed (or Rejected: done, but red)
        Current,  ///< Available, Assigned, Started or Escalated
        Upcoming, ///< not reached yet
        Skipped   ///< passed by: a later stop was reached, or the item was cancelled
    };

    std::string id;
    std::string name;
    std::string label;   ///< at most 8 characters, shown under the station
    std::string state;   ///< exact state name
    std::string assignee;
    Kind kind;
    bool rejected;

    Station() : kind(Kind::Upcoming), rejected(false) {}
};

/** @brief An instance's route: main line, exception branch and where the train is. */
struct RouteModel {
    std::vector<Station> main;
    std::vector<Station> branch; ///< exception stops, in definition order
    int split;                   ///< main index the branch drops from, -1 if there is no branch
    bool branchTaken;            ///< any exception stop has been reached
    int current;                 ///< main index of the current stop, -1 if none
    int branchCurrent;           ///< branch index of the current exception stop, -1 if none
    int done;                    ///< main stops done
    bool arrived;                ///< every main stop is done or passed, and nothing is current

    RouteModel() : split(-1), branchTaken(false), current(-1), branchCurrent(-1), done(0), arrived(false) {}

    /**
     * @brief Builds the route of @p items (definition order) using @p map (may be empty).
     * @param finished the instance is complete: stops it never reached are not needed (Skipped)
     */
    static RouteModel build(const std::vector<WorkItemView>& items, const std::vector<RouteStop>& map,
                            bool finished = false);

    /** @brief The stop the train is at (branch first when the exception route is taken), or 0. */
    const Station* currentStation() const;
    /** @brief The next main stop after the current one that is not done, or 0 for the last stop. */
    const Station* nextStation() const;
    /** @brief The most recent stop reached: current, else the last done. 0 before the first. */
    const Station* lastReached() const;
};

/**
 * @brief Up to 8 cells for a station label: @p given if the engine provides one, else
 *        the first CamelCase word of @p name ("SelectDeliveryType" gives "Select"). A longer
 *        word is cut to 7 characters and "…".
 */
std::string stationLabel(const std::string& name, const std::string& given);

/** @brief Columns per station needed for the labelled route line. */
const int kStationSpacing = 10;

/** @brief Columns of each main-line station when drawn in a box @p innerWidth wide; empty if they don't fit. */
std::vector<int> stationColumns(int stationCount, int innerWidth);

/**
 * @brief Draws @p route in the route box @p window (inner rows 1..7).
 * @param travelTo while animating: the travelled track stops at this column and the
 *                 current stop ◉ is drawn there instead of at its station; -1 when not animating
 * @param branchFill while animating the exception route: how many of its cells are filled, with a
 *                   red ◉ travelling at the head of the fill; -1 for all
 */
void drawRoute(void* window, const RouteModel& route, int travelTo = -1, int branchFill = -1);

/**
 * @brief Cells of the exception branch path (the two drops, then the track from the bend)
 *        up to and including branch stop @p stop, when drawn in @p window. 0 if there is no
 *        branch or the stations don't fit. Used to time the red fill animation.
 */
int branchCellsTo(void* window, const RouteModel& route, int stop);

/** @brief Compact route as glyphs with @p dashes ━ between stops, e.g. ●━━●━━◉━━○ (UTF-8). */
std::vector<std::pair<std::string, Tone> > miniRoute(const RouteModel& route, int dashes);

} // namespace ui
} // namespace tividy

#endif
