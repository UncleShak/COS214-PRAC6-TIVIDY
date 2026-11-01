/**
 * @file TuiFrontEnd.h
 * @brief The ncurses front end: draws the screens and turns key presses into controller calls.
 *
 * No engine or workflow logic lives here. Everything the user does goes through
 * InterfaceController, and everything drawn comes from its SessionView.
 *
 * Screens (docs/tui-plan.md section 4):
 * - Industries: pick the industry to work in.
 * - Instances: the industry's workflow instances; start a new one with a form.
 * - Dashboard: work items and their states; actions, approvals, the clock, and a result
 *   panel showing each state change and routing decision (or the engine's refusal).
 * - History: the instance's history through Envy's HistoryIterator (order, filter).
 * - Reports: the Progress, Audit and Overdue visitor reports.
 * Plus help (?), a command bar (:) that accepts the same commands as --plain, events (e)
 * with a full-screen alert, undo (u), and the "terminal too small" screen.
 *
 * Look (Persona 3 Reload style, kept simple): navy background, a title band and a calendar
 * strip at the top, a slanted command menu on the left (wide terminals), cyan selection bars,
 * slanted panel tabs, and a live event ticker above the key hints.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_TUI_FRONT_END_H
#define TIVIDY_TUI_FRONT_END_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <curses.h>

#include "InterfaceController.h"
#include "Layout.h"
#include "LineInterpreter.h"
#include "RouteLine.h"
#include "Theme.h"
#include "Widgets.h"

namespace tividy {
namespace ui {

/** @brief Event loop of the ncurses interface. Requires an active CursesSession. */
class TuiFrontEnd {
public:
    /** @param engineLabel shown in the header so a fake engine is never mistaken for the real one */
    TuiFrontEnd(InterfaceController& controller, const std::string& engineLabel);

    /** @brief Runs until the user quits. */
    void run();

private:
    enum class Screen { Industries, Instances, Dashboard, History, Reports };
    enum class Pane { List, Detail };

    // ---- keys
    /** @return true if @p key was a global key and has been handled */
    bool handleGlobal(int key);
    void handleIndustries(int key);
    void handleInstances(int key);
    void handleDashboard(int key);
    void handleHistory(int key);
    void handleReports(int key);
    /** @brief Up/Down/PgUp/PgDn/Home/End on a list of @p count rows. @return true if handled */
    bool moveSelection(int key, int count);
    /** @brief The same keys, scrolling @p top instead of moving a selection. */
    static bool scrollBy(int key, int& top);

    // ---- actions
    void enter(Screen screen);
    /** @brief After a typed command, shows the screen matching what the controller has selected. */
    void followController();
    void openIndustry();
    void openInstance();
    void newInstance();
    void chooseActor();
    void actionMenu();
    void approvalsMenu();
    void advanceClock();
    void raiseEvent();
    void undo();
    void runCommand();
    void showHelp();
    void quit();
    void loadHistory();
    void loadReports();
    /** @brief Fills the result panel from the controller's last outcome. */
    void showOutcome();
    /** @brief Fills the result panel with a refusal and the item's (unchanged) state. */
    void showRefusal(const std::string& itemId, const std::string& stateBefore, const std::string& message);
    /** @brief Selects the dashboard row of @p itemId, if it is still listed. */
    void reselect(const std::string& itemId);

    // ---- what the screens show
    const std::vector<WorkItemView>& items() const;
    /** @brief Indexes into items() in display order: main line first, then the exception route. */
    std::vector<int> itemOrder() const;
    const WorkItemView* selectedItem() const;
    /** @brief The engine's route map of a definition, fetched once and kept. */
    const std::vector<RouteStop>& routeMapFor(const std::string& industryId, const std::string& definition) const;
    /** @brief The selected instance's route line. */
    RouteModel currentRoute() const;
    /** @brief True if @p itemId is on an exception route of the selected instance's definition. */
    bool isException(const std::string& itemId) const;
    const WorkItemView* findItem(const std::string& itemId) const;
    std::string industryName() const;
    int rowCount() const;
    /**
     * @brief The list's display rows.
     * @param rowIndex set to the selectable index of each row, -1 for a heading
     */
    std::vector<Line> listRows(int width, std::vector<int>& rowIndex) const;
    std::vector<Line> detailLines() const;
    std::vector<Line> bodyLines() const;
    std::string listTitle() const;
    /** @brief Right side of the list's top edge, e.g. "3 of 13 done". */
    std::string listSummary() const;
    std::string emptyListText() const;
    /** @brief Header breadcrumbs after TIVIDY: industry, instance, screen. */
    std::vector<std::string> breadcrumbs() const;
    /** @brief Footer hints as (key, label) pairs, drawn as small key bullets. */
    std::vector<std::pair<std::string, std::string> > hints() const;

    // ---- drawing
    /** @brief Index of the industry whose line colour is shown (the cursor's, on the Industries screen). */
    int lineIndustry() const;
    void draw();
    /** @brief Only a centred "make the window bigger" message. */
    void drawTooSmall();
    /** @brief Two-row band: route bullet, TIVIDY and breadcrumbs, industry bullets; then context. */
    void drawHeader();
    /** @brief The full-width route box at the top of the dashboard. */
    void drawRoute();
    void drawMenu();
    /** @brief The one-row announcement bar (events, errors, confirmations). */
    void drawAnnounce();
    void drawPanes();
    void drawBody();
    /** @brief The key hints. */
    void drawFooter();
    /** @brief The list window of the current screen (dashboard or page). */
    WINDOW* listWindow() const;
    /** @brief The detail window of the current screen (dashboard or page). */
    WINDOW* detailWindow() const;

    InterfaceController& controller_;
    LineInterpreter interpreter_;
    std::string engineLabel_;
    Layout layout_;
    StatusMessage status_;

    Screen screen_;
    Pane focus_;
    int selected_;
    int listTop_;
    int detailTop_;
    int bodyTop_;
    bool availableOnly_;  ///< dashboard lists the available work only

    bool showPanel_;      ///< dashboard detail pane shows the result panel
    std::string panelTitle_;
    std::vector<Line> panel_;

    std::vector<std::string> historyLines_;
    bool newestFirst_;
    std::string filter_;

    int reportTab_;       ///< 0 Progress, 1 Audit, 2 Overdue
    std::string progress_;
    std::vector<std::string> audit_;
    std::vector<std::string> overdue_;

    bool running_;

    mutable std::map<std::string, std::vector<RouteStop> > routeMaps_;
};

} // namespace ui
} // namespace tividy

#endif
