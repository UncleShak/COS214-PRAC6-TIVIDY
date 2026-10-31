#include "TuiFrontEnd.h"

#include <algorithm>
#include <cstdio>

#include "InputValidation.h"
#include "Log.h"

namespace tividy {
namespace ui {

namespace {

const int kCtrlL = 12;

/** Every state name, in the order the Progress report lists them. */
const char* const kStates[] = {"Created", "Available", "Assigned", "Started",
                               "Completed", "Escalated", "Rejected", "Cancelled"};
const int kStateCount = 8;

const char* const kReportTabs[] = {"Progress", "Audit", "Overdue"};
const int kReportTabCount = 3;

const WorkAction kActions[] = {WorkAction::Assign, WorkAction::Start, WorkAction::Complete,
                               WorkAction::Reject, WorkAction::Cancel, WorkAction::Escalate};
const int kActionCount = 6;

std::string number(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        out += (i == 0 ? "" : separator) + values[i];
    }
    return out;
}

std::string orDash(const std::string& text) {
    return text.empty() ? "-" : text;
}

std::string instanceStatus(const InstanceView& instance) {
    if (instance.complete) {
        return "complete";
    }
    return instance.stalled ? "stalled" : "running";
}

Tone instanceTone(const InstanceView& instance) {
    if (instance.complete) {
        return Tone::Muted;
    }
    return instance.stalled ? Tone::Red : Tone::Gold;
}

bool isFinal(const std::string& state) {
    return state == "Completed" || state == "Rejected" || state == "Cancelled";
}

bool hasRole(const ParticipantView& person, const std::string& role) {
    return std::find(person.roles.begin(), person.roles.end(), role) != person.roles.end();
}

std::string describePerson(const ParticipantView& person) {
    return person.name + "   roles: " + orDash(join(person.roles, ", ")) + "   workload: " +
           number(person.workload);
}

bool isBack(int key) {
    return key == 'b' || key == kKeyEscape || key == KEY_LEFT;
}

} // namespace

TuiFrontEnd::TuiFrontEnd(InterfaceController& controller, const std::string& engineLabel)
    : controller_(controller), interpreter_(controller), engineLabel_(engineLabel),
      status_(controller.status()), screen_(Screen::Industries), focus_(Pane::List), selected_(0),
      listTop_(0), detailTop_(0), bodyTop_(0), availableOnly_(false), showPanel_(false),
      newestFirst_(false), reportTab_(0), running_(true) {
    enter(Screen::Industries);
}

void TuiFrontEnd::run() {
    draw();
    while (running_) {
        const int key = getch();
        if (key == KEY_RESIZE) {
            Log::write("resize");
            layout_.rebuild();
        } else if (layout_.tooSmall()) {
            if (key == 'q' || key == 'Q') {
                running_ = false;
            }
        } else if (!handleGlobal(key)) {
            switch (screen_) {
            case Screen::Industries: handleIndustries(key); break;
            case Screen::Instances:  handleInstances(key); break;
            case Screen::Dashboard:  handleDashboard(key); break;
            case Screen::History:    handleHistory(key); break;
            case Screen::Reports:    handleReports(key); break;
            }
        }
        if (running_) {
            draw();
        }
    }
}

// ---- keys -------------------------------------------------------------------------------

bool TuiFrontEnd::handleGlobal(int key) {
    const bool working = screen_ == Screen::Instances || screen_ == Screen::Dashboard;
    switch (key) {
    case '?':
        showHelp();
        return true;
    case ':':
        runCommand();
        return true;
    case 'q':
    case 'Q':
        quit();
        return true;
    case kCtrlL:
        clearok(curscr, TRUE);
        return true;
    case 'p':
        if (working) {
            chooseActor();
            return true;
        }
        return false;
    case 't':
        if (working) {
            advanceClock();
            return true;
        }
        return false;
    case 'e':
        if (screen_ == Screen::Dashboard) {
            raiseEvent();
            return true;
        }
        return false;
    case 'u':
        if (screen_ == Screen::Dashboard) {
            undo();
            return true;
        }
        return false;
    default:
        return false;
    }
}

void TuiFrontEnd::handleIndustries(int key) {
    if (isEnter(key) || key == KEY_RIGHT || key == 'l') {
        openIndustry();
    } else {
        moveSelection(key, rowCount());
    }
}

void TuiFrontEnd::handleInstances(int key) {
    if (isEnter(key) || key == KEY_RIGHT || key == 'l') {
        openInstance();
    } else if (key == 'n') {
        newInstance();
    } else if (isBack(key) || key == 'i') {
        enter(Screen::Industries);
    } else {
        moveSelection(key, rowCount());
    }
}

void TuiFrontEnd::handleDashboard(int key) {
    if (showPanel_) {
        showPanel_ = false;
        detailTop_ = 0;
        if (isEnter(key) || key == kKeyEscape || key == ' ') {
            return; // these keys only close the result panel
        }
    }

    if (key == '\t') {
        focus_ = focus_ == Pane::List ? Pane::Detail : Pane::List;
    } else if (focus_ == Pane::Detail && scrollBy(key, detailTop_)) {
        return;
    } else if (isEnter(key)) {
        actionMenu();
    } else if (key == 'v') {
        const WorkItemView* item = selectedItem();
        const std::string id = item != 0 ? item->id : "";
        availableOnly_ = !availableOnly_;
        selected_ = 0;
        reselect(id);
        status_ = StatusMessage::info(availableOnly_ ? "Showing available work only" : "Showing all work items");
    } else if (key == 'a') {
        approvalsMenu();
    } else if (key == 'h') {
        enter(Screen::History);
    } else if (key == 'r') {
        enter(Screen::Reports);
    } else if (key == 'n') {
        newInstance();
    } else if (isBack(key)) {
        enter(Screen::Instances);
    } else if (moveSelection(key, rowCount())) {
        detailTop_ = 0;
    }
}

void TuiFrontEnd::handleHistory(int key) {
    if (key == 'o') {
        newestFirst_ = !newestFirst_;
        loadHistory();
    } else if (key == '/') {
        std::string text = filter_;
        if (Dialog::prompt("FILTER HISTORY", "Show only entries containing (empty for all):", text)) {
            filter_ = trim(text);
            loadHistory();
        }
    } else if (key == kKeyEscape && !filter_.empty()) {
        filter_.clear();
        loadHistory();
    } else if (isBack(key) || key == 'h') {
        enter(Screen::Dashboard);
    } else {
        scrollBy(key, bodyTop_);
    }
}

void TuiFrontEnd::handleReports(int key) {
    if (key == KEY_RIGHT || key == '\t' || key == 'l') {
        reportTab_ = (reportTab_ + 1) % kReportTabCount;
        bodyTop_ = 0;
    } else if (key == KEY_LEFT && reportTab_ > 0) {
        --reportTab_;
        bodyTop_ = 0;
    } else if (key == KEY_BTAB) {
        reportTab_ = (reportTab_ + kReportTabCount - 1) % kReportTabCount;
        bodyTop_ = 0;
    } else if (key == 'r') {
        loadReports();
    } else if (isBack(key)) {
        enter(Screen::Dashboard);
    } else {
        scrollBy(key, bodyTop_);
    }
}

bool TuiFrontEnd::moveSelection(int key, int count) {
    const int page = layout_.list() != 0 ? std::max(1, getmaxy(layout_.list()) - 2) : 10;
    int next = selected_;
    switch (key) {
    case KEY_UP:
    case 'k':   next = selected_ - 1; break;
    case KEY_DOWN:
    case 'j':   next = selected_ + 1; break;
    case KEY_PPAGE: next = selected_ - page; break;
    case KEY_NPAGE: next = selected_ + page; break;
    case KEY_HOME:  next = 0; break;
    case KEY_END:   next = count - 1; break;
    default:
        return false;
    }
    selected_ = std::max(0, std::min(next, count - 1));
    return true;
}

bool TuiFrontEnd::scrollBy(int key, int& top) {
    const int page = std::max(1, LINES - 8);
    switch (key) {
    case KEY_UP:
    case 'k':   --top; break;
    case KEY_DOWN:
    case 'j':   ++top; break;
    case KEY_PPAGE: top -= page; break;
    case KEY_NPAGE: top += page; break;
    case KEY_HOME:  top = 0; break;
    case KEY_END:   top = 1 << 20; break; // clamped when drawn
    default:
        return false;
    }
    top = std::max(0, top);
    return true;
}

// ---- actions ----------------------------------------------------------------------------

void TuiFrontEnd::enter(Screen screen) {
    const SessionView& view = controller_.view();
    screen_ = screen;
    focus_ = Pane::List;
    selected_ = 0;
    listTop_ = 0;
    detailTop_ = 0;
    bodyTop_ = 0;
    showPanel_ = false;

    switch (screen) {
    case Screen::Industries:
        for (std::size_t i = 0; i < view.industries.size(); ++i) {
            if (view.industries[i].id == view.industryId) {
                selected_ = static_cast<int>(i);
            }
        }
        break;
    case Screen::Instances:
        for (std::size_t i = 0; i < view.instances.size(); ++i) {
            if (view.instances[i].id == view.instanceId) {
                selected_ = static_cast<int>(i);
            }
        }
        break;
    case Screen::Dashboard:
        break;
    case Screen::History:
        loadHistory();
        break;
    case Screen::Reports:
        loadReports();
        break;
    }
}

void TuiFrontEnd::followController() {
    const SessionView& view = controller_.view();
    if (!view.instanceId.empty()) {
        if (screen_ == Screen::Industries || screen_ == Screen::Instances) {
            enter(Screen::Dashboard);
        }
    } else if (!view.industryId.empty()) {
        if (screen_ != Screen::Instances) {
            enter(Screen::Instances);
        }
    } else if (screen_ != Screen::Industries) {
        enter(Screen::Industries);
    }
}

void TuiFrontEnd::openIndustry() {
    const std::vector<IndustryInfo>& industries = controller_.view().industries;
    if (selected_ >= static_cast<int>(industries.size())) {
        status_ = StatusMessage::invalidInput("no industry to open");
        return;
    }
    status_ = controller_.selectIndustry(industries[selected_].id);
    if (!status_.isError()) {
        enter(Screen::Instances);
    }
}

void TuiFrontEnd::openInstance() {
    const std::vector<InstanceView>& instances = controller_.view().instances;
    if (selected_ >= static_cast<int>(instances.size())) {
        status_ = StatusMessage::invalidInput("there are no instances yet: press n to start one");
        return;
    }
    status_ = controller_.selectInstance(instances[selected_].id);
    if (!status_.isError()) {
        availableOnly_ = false;
        enter(Screen::Dashboard);
    }
}

void TuiFrontEnd::newInstance() {
    const SessionView& view = controller_.view();
    std::vector<std::string> definitions;
    for (std::size_t i = 0; i < view.industries.size(); ++i) {
        if (view.industries[i].id == view.industryId) {
            definitions = view.industries[i].definitions;
        }
    }
    if (definitions.empty()) {
        status_ = StatusMessage::invalidInput("this industry has no workflow definitions to start");
        return;
    }

    std::string definition = definitions.front();
    if (definitions.size() > 1) {
        std::vector<Line> entries;
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            entries.push_back(Line(definitions[i]));
        }
        const int pick = Dialog::menu("START WHICH WORKFLOW?", entries);
        if (pick == Dialog::kCancelled) {
            status_ = StatusMessage::info("Start cancelled, nothing was created");
            return;
        }
        definition = definitions[pick];
        draw();
    }

    std::vector<DataField> fields;
    status_ = controller_.startFields(definition, fields);
    if (status_.isError()) {
        return;
    }
    std::map<std::string, std::string> values;
    if (!Dialog::form("START: " + definition, fields, values)) {
        status_ = StatusMessage::info("Start cancelled, nothing was created");
        return;
    }
    status_ = controller_.startInstance(definition, values);
    if (!status_.isError()) {
        availableOnly_ = false;
        enter(Screen::Dashboard);
    }
}

void TuiFrontEnd::chooseActor() {
    const SessionView& view = controller_.view();
    if (view.participants.empty()) {
        status_ = StatusMessage::invalidInput("this industry has no participants");
        return;
    }
    std::vector<Line> entries;
    int current = 0;
    for (std::size_t i = 0; i < view.participants.size(); ++i) {
        entries.push_back(Line(describePerson(view.participants[i])));
        if (view.participants[i].id == view.actorId) {
            current = static_cast<int>(i);
        }
    }
    const int pick = Dialog::menu("ACT AS", entries, current);
    if (pick != Dialog::kCancelled) {
        status_ = controller_.setActor(view.participants[pick].id);
    }
}

void TuiFrontEnd::actionMenu() {
    const WorkItemView* selected = selectedItem();
    if (selected == 0) {
        status_ = StatusMessage::invalidInput("no work item is selected");
        return;
    }
    const WorkItemView item = *selected; // the view is rebuilt by the action

    const std::vector<WorkAction> hinted = InterfaceController::hintedActions(item.state);
    std::vector<Line> entries;
    int first = -1;
    for (int i = 0; i < kActionCount; ++i) {
        const bool likely = std::find(hinted.begin(), hinted.end(), kActions[i]) != hinted.end();
        std::string text = actionName(kActions[i]);
        if (kActions[i] == WorkAction::Assign || kActions[i] == WorkAction::Reject) {
            text += "...";
        }
        if (likely) {
            entries.push_back(Line(text));
            if (first < 0) {
                first = i;
            }
        } else {
            entries.push_back(Line(fit(text, 12) + "(not allowed from " + item.state + ")", Tone::Muted));
        }
    }
    const int pick = Dialog::menu(item.name + " (" + item.state + ")", entries, first < 0 ? 0 : first);
    if (pick == Dialog::kCancelled) {
        return;
    }
    const WorkAction action = kActions[pick];

    std::string assignee;
    std::string reason;
    if (action == WorkAction::Assign) {
        draw();
        const std::vector<ParticipantView>& people = controller_.view().participants;
        std::vector<Line> choices;
        choices.push_back(Line("Auto (the industry's assignment rule chooses)", Tone::Gold));
        for (std::size_t i = 0; i < people.size(); ++i) {
            const bool fits = item.requiredRole.empty() || hasRole(people[i], item.requiredRole);
            choices.push_back(Line(describePerson(people[i]), fits ? Tone::Bone : Tone::Muted));
        }
        const int who = Dialog::menu("ASSIGN '" + item.name + "' (needs " + orDash(item.requiredRole) + ")", choices);
        if (who == Dialog::kCancelled) {
            status_ = StatusMessage::info("Assign cancelled, nothing changed");
            return;
        }
        assignee = who == 0 ? "" : people[who - 1].id;
    } else if (action == WorkAction::Reject) {
        draw();
        if (!Dialog::prompt("REJECT '" + item.name + "'", "Reason (required):", reason)) {
            status_ = StatusMessage::info("Reject cancelled, nothing changed");
            return;
        }
    } else if (action == WorkAction::Cancel) {
        draw();
        if (!Dialog::confirm("Cancel '" + item.name + "'? (u undoes it)")) {
            status_ = StatusMessage::info("Cancel not confirmed, nothing changed");
            return;
        }
    }

    status_ = controller_.perform(action, item.id, assignee, reason);
    if (status_.isError()) {
        showRefusal(item.id, item.state, status_.text);
    } else {
        showOutcome();
    }
    reselect(item.id);
}

void TuiFrontEnd::approvalsMenu() {
    const std::vector<ApprovalView> approvals = controller_.view().approvals;
    if (approvals.empty()) {
        status_ = StatusMessage::info("No approval requests in this instance");
        return;
    }
    std::vector<Line> entries;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        const ApprovalView& a = approvals[i];
        std::string text = fit(a.id, 8) + fit(a.status, 10) + fit("'" + a.workItemId + "'", 20) + "needs " +
                           fit(a.approverRole, 10) + a.summary;
        if (!a.decidedBy.empty()) {
            text += "  (by " + a.decidedBy + (a.reason.empty() ? "" : ": " + a.reason) + ")";
        }
        entries.push_back(Line(text, a.status == "Pending" ? Tone::Gold : Tone::Muted));
    }
    const int pick = Dialog::menu("APPROVALS", entries);
    if (pick == Dialog::kCancelled) {
        return;
    }
    const ApprovalView approval = approvals[pick];
    const Tone tone = approval.status == "Pending" ? Tone::Bone : Tone::Muted;

    draw();
    std::vector<Line> choices;
    choices.push_back(Line("Approve", tone));
    choices.push_back(Line("Reject...", tone));
    const int decision = Dialog::menu(approval.id + ": " + approval.summary, choices);
    if (decision == Dialog::kCancelled) {
        return;
    }

    const WorkItemView* item = findItem(approval.workItemId);
    const std::string stateBefore = item != 0 ? item->state : "";
    if (decision == 0) {
        status_ = controller_.approve(approval.id);
    } else {
        draw();
        std::string reason;
        if (!Dialog::prompt("REJECT " + approval.id, "Reason (required):", reason)) {
            status_ = StatusMessage::info("Reject cancelled, nothing changed");
            return;
        }
        status_ = controller_.rejectApproval(approval.id, reason);
    }
    if (status_.isError()) {
        showRefusal(approval.workItemId, stateBefore, status_.text);
    } else {
        showOutcome();
    }
}

void TuiFrontEnd::advanceClock() {
    std::string units = "1";
    if (!Dialog::prompt("ADVANCE CLOCK",
                        "The clock is at t=" + number(controller_.view().clock) +
                            ". Move it forward by how many units? (1 to 1000)",
                        units, 4)) {
        return;
    }
    std::vector<std::string> changes;
    status_ = controller_.advance(units, changes);
    if (status_.isError()) {
        return;
    }
    std::vector<Line> lines;
    lines.push_back(Line(status_.text, Tone::Gold));
    lines.push_back(Line());
    for (std::size_t i = 0; i < changes.size(); ++i) {
        lines.push_back(Line(changes[i], Tone::Red));
    }
    if (screen_ == Screen::Dashboard) {
        panelTitle_ = "CLOCK";
        panel_ = lines;
        panel_.push_back(Line());
        panel_.push_back(Line("Enter: back to the work item", Tone::Muted));
        showPanel_ = true;
        detailTop_ = 0;
    } else if (!changes.empty()) {
        draw();
        Dialog::text("CLOCK", lines);
    }
}

void TuiFrontEnd::raiseEvent() {
    const std::vector<std::string> types = controller_.eventTypes();
    if (types.empty()) {
        status_ = StatusMessage::info("This industry raises no events");
        return;
    }
    std::vector<Line> entries;
    for (std::size_t i = 0; i < types.size(); ++i) {
        entries.push_back(Line(types[i], Tone::Red));
    }
    const int pick = Dialog::menu("RAISE AN EVENT", entries);
    if (pick == Dialog::kCancelled) {
        return;
    }
    draw();
    std::string description;
    if (!Dialog::prompt(types[pick], "What happened? (optional, shown in the history)", description)) {
        status_ = StatusMessage::info("Event not raised, nothing changed");
        return;
    }
    std::vector<std::string> changes;
    status_ = controller_.raiseEvent(types[pick], description, changes);
    if (status_.isError()) {
        return;
    }
    std::vector<Line> lines;
    for (std::size_t i = 0; i < changes.size(); ++i) {
        const bool cancelled = changes[i].find("Cancelled") != std::string::npos;
        lines.push_back(Line((cancelled ? "x  " : "+  ") + changes[i], cancelled ? Tone::Red : Tone::Gold));
    }
    if (lines.empty()) {
        lines.push_back(Line("Nothing changed: no open work reacts to this event.", Tone::Muted));
    }
    lines.push_back(Line());
    lines.push_back(Line("u undoes it", Tone::Muted));
    Dialog::alert(types[pick], description.empty() ? controller_.view().instanceId : description, lines);
    layout_.rebuild();   // the alert covered the whole screen: repaint every window from scratch
}

void TuiFrontEnd::undo() {
    const std::string next = controller_.nextUndo();
    if (next.empty()) {
        status_ = StatusMessage::info("Nothing to undo in this instance");
        return;
    }
    if (!Dialog::confirm("Undo '" + next + "'?")) {
        status_ = StatusMessage::info("Undo not confirmed, nothing changed");
        return;
    }
    const WorkItemView* item = selectedItem();
    const std::string id = item != 0 ? item->id : "";
    status_ = controller_.undo();
    reselect(id);
}

void TuiFrontEnd::runCommand() {
    std::string line;
    if (!Dialog::prompt("COMMAND", "Type a command (help lists them; same commands as --plain):", line) ||
        trim(line).empty()) {
        return;
    }
    Log::write("command: " + line);
    const Reply reply = interpreter_.run(line);
    status_ = reply.status;
    if (reply.quit) {
        running_ = false;
        return;
    }
    followController();
    if (!reply.lines.empty()) {
        draw();
        std::vector<Line> lines;
        for (std::size_t i = 0; i < reply.lines.size(); ++i) {
            lines.push_back(Line(reply.lines[i]));
        }
        Dialog::text(":" + trim(line), lines);
    }
}

void TuiFrontEnd::showHelp() {
    std::vector<Line> lines;
    lines.push_back(Line("Everywhere", Tone::Gold));
    lines.push_back(Line("  Up/Down or j/k   move              PgUp/PgDn, Home/End   jump"));
    lines.push_back(Line("  Enter            open or choose    b, Esc, Left          back"));
    lines.push_back(Line("  :                type a command (the same commands as tividy --plain)"));
    lines.push_back(Line("  ?                this help         q                     quit (asks first)"));
    lines.push_back(Line("  Ctrl+L           redraw            Ctrl+C                quit at once"));
    lines.push_back(Line());

    switch (screen_) {
    case Screen::Industries:
        lines.push_back(Line("Industries", Tone::Gold));
        lines.push_back(Line("  Enter            work in the selected industry"));
        break;
    case Screen::Instances:
        lines.push_back(Line("Instances", Tone::Gold));
        lines.push_back(Line("  Enter            open the selected instance"));
        lines.push_back(Line("  n                start a new instance (asks for its start data)"));
        lines.push_back(Line("  p                choose who you are acting as"));
        lines.push_back(Line("  t                move the engine clock forward"));
        lines.push_back(Line("  i                change industry"));
        break;
    case Screen::Dashboard:
        lines.push_back(Line("Instance dashboard", Tone::Gold));
        lines.push_back(Line("  Enter            actions on the selected work item"));
        lines.push_back(Line("                   (muted actions would be refused; you can still try them)"));
        lines.push_back(Line("  v                all work items / available work only"));
        lines.push_back(Line("  a                approval requests (approve or reject)"));
        lines.push_back(Line("  h                history          r   reports"));
        lines.push_back(Line("  n                start another instance"));
        lines.push_back(Line("  p                choose who you are acting as"));
        lines.push_back(Line("  t                move the engine clock forward (deadlines, escalation)"));
        lines.push_back(Line("  e                raise an event (e.g. WORM_SIGN); interrupt rules react"));
        lines.push_back(Line("  u                undo the last action in this instance"));
        lines.push_back(Line("  Tab              switch between the list and the detail pane"));
        lines.push_back(Line());
        lines.push_back(Line("Colours: cyan = active work, steel = done or waiting, red = rejected,", Tone::Muted));
        lines.push_back(Line("cancelled, escalated or an error. The cyan bar is the selection.", Tone::Muted));
        break;
    case Screen::History:
        lines.push_back(Line("History", Tone::Gold));
        lines.push_back(Line("  o                oldest first / newest first"));
        lines.push_back(Line("  /                show only entries containing some text"));
        lines.push_back(Line("  Esc              clear the filter (or go back if there is none)"));
        break;
    case Screen::Reports:
        lines.push_back(Line("Reports", Tone::Gold));
        lines.push_back(Line("  Left/Right, Tab  Progress, Audit, Overdue"));
        lines.push_back(Line("  r                reload the reports"));
        break;
    }
    Dialog::text("HELP", lines);
}

void TuiFrontEnd::quit() {
    if (Dialog::confirm("Quit TIVIDY?")) {
        running_ = false;
    }
}

void TuiFrontEnd::loadHistory() {
    status_ = controller_.history(historyLines_, newestFirst_, filter_);
    bodyTop_ = 0;
}

void TuiFrontEnd::loadReports() {
    progress_.clear();
    audit_.clear();
    overdue_.clear();
    status_ = controller_.reports(progress_, audit_, overdue_);
    bodyTop_ = 0;
}

void TuiFrontEnd::showOutcome() {
    const ActionOutcome& outcome = controller_.lastOutcome();
    panelTitle_ = "RESULT";
    panel_.clear();
    panel_.push_back(Line(outcome.commandDescription, Tone::Gold));
    panel_.push_back(Line());
    if (!outcome.previousState.empty() || !outcome.newState.empty()) {
        panel_.push_back(Line("State:          " + orDash(outcome.previousState) + " -> " + orDash(outcome.newState),
                              Theme::forState(outcome.newState, "")));
    }
    if (!outcome.routingDescription.empty()) {
        panel_.push_back(Line("Routing:        " + outcome.routingDescription, Tone::Gold));
    }
    if (!outcome.madeAvailable.empty()) {
        panel_.push_back(Line("Now available:  " + join(outcome.madeAvailable, ", ")));
    }
    if (!outcome.nowWaiting.empty()) {
        panel_.push_back(Line("Still waiting:  " + join(outcome.nowWaiting, ", "), Tone::Muted));
    }
    if (!outcome.reopened.empty()) {
        panel_.push_back(Line("Reopened:       " + join(outcome.reopened, ", ")));
    }
    if (outcome.instanceComplete) {
        panel_.push_back(Line());
        panel_.push_back(Line("The instance is complete.", Tone::Gold));
    }
    panel_.push_back(Line());
    panel_.push_back(Line("Enter: back to the work item", Tone::Muted));
    showPanel_ = true;
    detailTop_ = 0;
}

void TuiFrontEnd::showRefusal(const std::string& itemId, const std::string& stateBefore,
                              const std::string& message) {
    const bool inputProblem = message.compare(0, 14, "Invalid input:") == 0;
    panelTitle_ = "NOT DONE";
    panel_.clear();
    panel_.push_back(Line(message, Tone::Red));
    panel_.push_back(Line());

    const WorkItemView* now = findItem(itemId);
    if (now != 0) {
        if (stateBefore.empty() || now->state == stateBefore) {
            panel_.push_back(Line("State:  " + now->state + "  (unchanged)", Theme::forState(now->state, "")));
        } else {
            panel_.push_back(Line("State:  " + stateBefore + " -> " + now->state, Tone::Red));
        }
    }
    panel_.push_back(Line(inputProblem ? "Nothing was sent to the engine."
                                       : "The engine refused the action, so nothing changed.",
                          Tone::Muted));
    panel_.push_back(Line());
    panel_.push_back(Line("Enter: back to the work item", Tone::Muted));
    showPanel_ = true;
    detailTop_ = 0;
}

void TuiFrontEnd::reselect(const std::string& itemId) {
    const std::vector<WorkItemView>& rows = items();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].id == itemId) {
            selected_ = static_cast<int>(i);
            return;
        }
    }
    selected_ = std::max(0, std::min(selected_, static_cast<int>(rows.size()) - 1));
}

// ---- what the screens show --------------------------------------------------------------

const std::vector<WorkItemView>& TuiFrontEnd::items() const {
    return availableOnly_ ? controller_.view().availableWork : controller_.view().workItems;
}

const WorkItemView* TuiFrontEnd::selectedItem() const {
    const std::vector<WorkItemView>& rows = items();
    if (selected_ < 0 || selected_ >= static_cast<int>(rows.size())) {
        return 0;
    }
    return &rows[selected_];
}

const WorkItemView* TuiFrontEnd::findItem(const std::string& itemId) const {
    const std::vector<WorkItemView>& all = controller_.view().workItems;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].id == itemId) {
            return &all[i];
        }
    }
    return 0;
}

std::string TuiFrontEnd::industryName() const {
    const SessionView& view = controller_.view();
    for (std::size_t i = 0; i < view.industries.size(); ++i) {
        if (view.industries[i].id == view.industryId) {
            return view.industries[i].displayName;
        }
    }
    return view.industryId;
}

int TuiFrontEnd::rowCount() const {
    switch (screen_) {
    case Screen::Industries: return static_cast<int>(controller_.view().industries.size());
    case Screen::Instances:  return static_cast<int>(controller_.view().instances.size());
    case Screen::Dashboard:  return static_cast<int>(items().size());
    default:                 return 0;
    }
}

std::vector<Line> TuiFrontEnd::listRows(int width) const {
    const SessionView& view = controller_.view();
    std::vector<Line> rows;
    if (screen_ == Screen::Industries) {
        for (std::size_t i = 0; i < view.industries.size(); ++i) {
            rows.push_back(Line(view.industries[i].displayName));
        }
    } else if (screen_ == Screen::Instances) {
        const std::size_t nameWidth = static_cast<std::size_t>(std::max(8, width - 25));
        for (std::size_t i = 0; i < view.instances.size(); ++i) {
            const InstanceView& instance = view.instances[i];
            rows.push_back(Line(fit(instance.id, 12) + fit(instance.definitionName, nameWidth) + " " +
                                    instanceStatus(instance),
                                instanceTone(instance)));
        }
    } else if (screen_ == Screen::Dashboard) {
        const std::vector<WorkItemView>& work = items();
        const std::size_t nameWidth = static_cast<std::size_t>(std::max(8, width - 14));
        for (std::size_t i = 0; i < work.size(); ++i) {
            const WorkItemView& item = work[i];
            const bool waiting = item.trackerStatus == "waiting";
            rows.push_back(Line(fit(item.name, nameWidth) + " " + (waiting ? std::string("Waiting") : item.state),
                                Theme::forState(item.state, item.trackerStatus)));
        }
    }
    return rows;
}

std::vector<Line> TuiFrontEnd::detailLines() const {
    const SessionView& view = controller_.view();
    std::vector<Line> lines;

    if (screen_ == Screen::Industries) {
        if (selected_ < static_cast<int>(view.industries.size())) {
            const IndustryInfo& industry = view.industries[selected_];
            lines.push_back(Line(industry.displayName, Tone::Gold));
            lines.push_back(Line(industry.description));
            lines.push_back(Line());
            lines.push_back(Line("Workflow definitions:", Tone::Muted));
            for (std::size_t i = 0; i < industry.definitions.size(); ++i) {
                lines.push_back(Line("  " + industry.definitions[i]));
            }
            lines.push_back(Line());
            lines.push_back(Line("Enter: work in this industry", Tone::Muted));
        }
        return lines;
    }

    if (screen_ == Screen::Instances) {
        if (selected_ < static_cast<int>(view.instances.size())) {
            const InstanceView& instance = view.instances[selected_];
            lines.push_back(Line(instance.id, Tone::Gold));
            lines.push_back(Line());
            lines.push_back(Line("Workflow:    " + instance.definitionName +
                                 (instance.definitionVersion.empty() ? "" : "  v" + instance.definitionVersion)));
            lines.push_back(Line("Status:      " + instanceStatus(instance), instanceTone(instance)));
            lines.push_back(Line());
            lines.push_back(Line("Start data:", Tone::Muted));
            if (instance.data.empty()) {
                lines.push_back(Line("  (none)", Tone::Muted));
            }
            for (std::map<std::string, std::string>::const_iterator it = instance.data.begin();
                 it != instance.data.end(); ++it) {
                lines.push_back(Line("  " + it->first + " = " + it->second));
            }
            lines.push_back(Line());
            lines.push_back(Line("Enter: open   n: start another", Tone::Muted));
        } else {
            lines.push_back(Line("No instances in " + industryName() + " yet.", Tone::Muted));
            lines.push_back(Line());
            lines.push_back(Line("Press n to start one."));
        }
        return lines;
    }

    // Dashboard
    if (showPanel_) {
        return panel_;
    }
    const WorkItemView* item = selectedItem();
    if (item == 0) {
        lines.push_back(Line(availableOnly_ ? "No work is available right now." : "This instance has no work items.",
                             Tone::Muted));
        if (view.instance.complete) {
            lines.push_back(Line("The instance is complete.", Tone::Gold));
        } else if (view.instance.stalled) {
            lines.push_back(Line("The instance is stalled: nothing can move forward.", Tone::Red));
        }
        if (availableOnly_) {
            lines.push_back(Line());
            lines.push_back(Line("v: show all work items", Tone::Muted));
        }
        return lines;
    }

    lines.push_back(Line(item->name, Tone::Gold));
    lines.push_back(Line());
    lines.push_back(Line("State:        " + item->state, Theme::forState(item->state, item->trackerStatus)));
    lines.push_back(Line("Progress:     " + (item->trackerStatus.empty() ? std::string("not reached yet")
                                                                         : item->trackerStatus)));
    lines.push_back(Line("Role:         " + orDash(item->requiredRole)));
    lines.push_back(Line("Assignee:     " + orDash(item->assigneeName)));
    lines.push_back(Line("Waiting on:   " + orDash(join(item->waitingOn, ", ")),
                         item->waitingOn.empty() ? Tone::Bone : Tone::Muted));
    if (item->dueAt >= 0) {
        const bool overdue = view.clock > item->dueAt && !isFinal(item->state);
        lines.push_back(Line("Due:          t=" + number(item->dueAt) + (overdue ? "  OVERDUE" : ""),
                             overdue ? Tone::Red : Tone::Bone));
    } else {
        lines.push_back(Line("Due:          -"));
    }

    const std::vector<WorkAction> hinted = InterfaceController::hintedActions(item->state);
    std::string allowed;
    for (std::size_t i = 0; i < hinted.size(); ++i) {
        allowed += (i == 0 ? "" : ", ") + std::string(actionName(hinted[i]));
    }
    if (allowed.empty()) {
        allowed = isFinal(item->state) ? "nothing (final state)" : "nothing until it becomes available";
    }
    lines.push_back(Line("Allowed now:  " + allowed));

    bool heading = false;
    for (std::size_t i = 0; i < view.approvals.size(); ++i) {
        const ApprovalView& a = view.approvals[i];
        if (a.workItemId != item->id) {
            continue;
        }
        if (!heading) {
            lines.push_back(Line());
            lines.push_back(Line("Approvals (a to decide):", Tone::Muted));
            heading = true;
        }
        lines.push_back(Line("  " + fit(a.id, 8) + fit(a.status, 10) + a.summary + "  (needs " + a.approverRole + ")",
                             a.status == "Pending" ? Tone::Gold : (a.status == "Rejected" ? Tone::Red : Tone::Muted)));
    }

    lines.push_back(Line());
    lines.push_back(Line("Instance data:", Tone::Muted));
    for (std::map<std::string, std::string>::const_iterator it = view.instance.data.begin();
         it != view.instance.data.end(); ++it) {
        lines.push_back(Line("  " + it->first + " = " + it->second));
    }
    lines.push_back(Line());
    lines.push_back(Line("Enter: actions on this work item", Tone::Muted));
    return lines;
}

std::vector<Line> TuiFrontEnd::bodyLines() const {
    std::vector<Line> lines;
    if (screen_ == Screen::History) {
        for (std::size_t i = 0; i < historyLines_.size(); ++i) {
            char index[16];
            std::snprintf(index, sizeof(index), "%4d  ", static_cast<int>(i) + 1);
            lines.push_back(Line(index + historyLines_[i]));
        }
        if (lines.empty()) {
            lines.push_back(Line(filter_.empty() ? "No history yet." : "No entries contain '" + filter_ + "'.",
                                 Tone::Muted));
        }
        return lines;
    }

    // Reports
    if (reportTab_ == 0) {
        const std::vector<WorkItemView>& work = controller_.view().workItems;
        const int total = static_cast<int>(work.size());
        lines.push_back(Line(progress_.empty() ? "(no progress summary)" : progress_));
        lines.push_back(Line());
        const int barWidth = 30;
        for (int s = 0; s < kStateCount; ++s) {
            int count = 0;
            for (std::size_t i = 0; i < work.size(); ++i) {
                if (work[i].state == kStates[s]) {
                    ++count;
                }
            }
            const int filled = total == 0 ? 0 : count * barWidth / total;
            std::string bar;
            for (int b = 0; b < barWidth; ++b) {
                bar += b < filled ? "\u2588" : "\u2591";   // full block / light shade
            }
            lines.push_back(Line(fit(kStates[s], 12) + bar + "  " + number(count),
                                 count == 0 ? Tone::Muted : Theme::forState(kStates[s], "")));
        }
    } else if (reportTab_ == 1) {
        for (std::size_t i = 0; i < audit_.size(); ++i) {
            lines.push_back(Line(audit_[i]));
        }
        if (lines.empty()) {
            lines.push_back(Line("The audit report is empty.", Tone::Muted));
        }
    } else {
        for (std::size_t i = 0; i < overdue_.size(); ++i) {
            lines.push_back(Line(overdue_[i], Tone::Red));
        }
        if (lines.empty()) {
            lines.push_back(Line("No overdue work.", Tone::Muted));
        }
    }
    return lines;
}

std::string TuiFrontEnd::listTitle() const {
    switch (screen_) {
    case Screen::Industries: return "INDUSTRY";
    case Screen::Instances:  return "INSTANCES";
    default:                 break;
    }
    const SessionView& view = controller_.view();
    int done = 0;
    for (std::size_t i = 0; i < view.workItems.size(); ++i) {
        if (view.workItems[i].state == "Completed") {
            ++done;
        }
    }
    const int total = static_cast<int>(view.workItems.size());
    return std::string(availableOnly_ ? "AVAILABLE WORK" : "WORK ITEMS") + "  " + number(done) + "/" +
           number(total) + " done (" + number(total == 0 ? 0 : done * 100 / total) + "%), " +
           instanceStatus(view.instance);
}

std::string TuiFrontEnd::emptyListText() const {
    switch (screen_) {
    case Screen::Industries: return "No industries registered with the engine.";
    case Screen::Instances:  return "No instances yet. Press n to start one.";
    default:                 return availableOnly_ ? "No work available." : "No work items.";
    }
}

std::string TuiFrontEnd::breadcrumb() const {
    const SessionView& view = controller_.view();
    std::string path;
    if (screen_ != Screen::Industries) {
        path += " > " + industryName();
    }
    if (screen_ == Screen::Dashboard || screen_ == Screen::History || screen_ == Screen::Reports) {
        path += " > " + view.instanceId + " (" + view.instance.definitionName + ")";
    }
    if (screen_ == Screen::History) {
        path += " > History";
    } else if (screen_ == Screen::Reports) {
        path += " > Reports";
    }
    return path;
}

std::string TuiFrontEnd::headerRight() const {
    const SessionView& view = controller_.view();
    std::string right;
    if (screen_ != Screen::Industries) {
        right += "t=" + number(view.clock) + "  ";
        const std::string actor = controller_.participantName(view.actorId);
        if (!actor.empty()) {
            right += "as " + actor + "  ";
        }
    }
    return right + "[" + engineLabel_ + "]";
}

std::string TuiFrontEnd::hints() const {
    switch (screen_) {
    case Screen::Industries:
        return "Enter open   : command   ? help   q quit";
    case Screen::Instances:
        return "Enter open   n new instance   p act as   t clock   b back   : command   ? help   q quit";
    case Screen::Dashboard:
        if (showPanel_) {
            return "Enter back to detail   (any other key also works)";
        }
        return layout_.geometry().showMenu ? "Enter actions   v available/all   Tab detail   b back   ? help"
                                           : "Enter act v avail a approve e event u undo h hist r rep p as t clock ? help";
    case Screen::History:
        return "Up/Down scroll   o oldest/newest first   / filter   Esc clear filter   b back   ? help";
    case Screen::Reports:
        return "Left/Right switch report   Up/Down scroll   r reload   b back   ? help";
    }
    return "";
}

// ---- drawing ----------------------------------------------------------------------------

void TuiFrontEnd::draw() {
    if (layout_.tooSmall()) {
        drawTooSmall();
        return;
    }
    drawTitle();
    drawCalendar();
    drawMenu();
    if (screen_ == Screen::History || screen_ == Screen::Reports) {
        drawBody();
    } else {
        drawPanes();
    }
    drawTicker();
    drawStatus();
    doupdate();
}

void TuiFrontEnd::drawTooSmall() {
    Theme::paint(stdscr);
    erase();
    char size[96];
    std::snprintf(size, sizeof(size), "Terminal is %dx%d. TIVIDY needs at least %dx%d.", COLS, LINES,
                  Layout::kMinCols, Layout::kMinRows);
    putText(stdscr, 1, 1, size, Tone::Red);
    putText(stdscr, 3, 1, "Enlarge the window, or run:  tividy --plain", Tone::Bone);
    putText(stdscr, 5, 1, "q quits", Tone::Muted);
    wnoutrefresh(stdscr);
    doupdate();
}

void TuiFrontEnd::drawTitle() {
    WINDOW* w = layout_.title();
    werase(w);
    putText(w, 0, 0, std::string(static_cast<std::size_t>(COLS), ' '), Tone::Title, 0);
    putText(w, 0, 1, "TIVIDY", Tone::Title, 0);
    putGlyphs(w, 0, 8, "\u25C6", Tone::Title);
    const std::string label = "[" + engineLabel_ + "]";
    const int labelWidth = static_cast<int>(label.size()) + 2;
    const std::string path = screen_ == Screen::Industries ? " Se7en workflow engine" : breadcrumb();
    putText(w, 0, 10, path, Tone::Title, labelWidth + 2);
    putText(w, 0, std::max(10, COLS - labelWidth), label, Tone::Title, 0);
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawCalendar() {
    // The Persona calendar strip: the day (engine clock), where you are and who you are.
    const SessionView& view = controller_.view();
    WINDOW* w = layout_.calendar();
    werase(w);
    putText(w, 0, 0, std::string(static_cast<std::size_t>(COLS), ' '), Tone::Strip, 0);
    putGlyphs(w, 0, 0, "\u25E4", Tone::Gold);
    std::string strip = "  t=" + number(view.clock);
    if (!view.industryId.empty()) {
        strip += "   |   " + industryName();
    }
    if (!view.instanceId.empty()) {
        strip += "   |   " + view.instanceId + "  " + instanceStatus(view.instance);
    }
    const std::string actor = controller_.participantName(view.actorId);
    if (!actor.empty()) {
        strip += "   |   acting as " + actor;
    }
    const std::string next = screen_ == Screen::Dashboard ? controller_.nextUndo() : "";
    const std::string undoText = next.empty() ? "" : "u: undo " + next + " ";
    // The undo hint keeps its place on the right; the strip is cut to fit beside it.
    const int undoWidth = std::min(static_cast<int>(undoText.size()), COLS / 3);
    putText(w, 0, 1, strip, Tone::Strip, undoWidth + 1);
    if (!undoText.empty()) {
        putText(w, 0, COLS - undoWidth, undoText, Tone::Strip, 0);
    }
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawMenu() {
    WINDOW* w = layout_.menu();
    if (w == 0) {
        return;
    }
    werase(w);
    const bool inInstance = !controller_.view().instanceId.empty();
    const bool inIndustry = !controller_.view().industryId.empty();
    struct Entry { const char* label; const char* key; bool screen; Screen target; bool enabled; };
    const Entry entries[] = {
        {"INDUSTRY",  "",  true,  Screen::Industries, true},
        {"INSTANCES", "b", true,  Screen::Instances,  inIndustry},
        {"DASHBOARD", "",  true,  Screen::Dashboard,  inInstance},
        {"HISTORY",   "h", true,  Screen::History,    inInstance},
        {"REPORTS",   "r", true,  Screen::Reports,    inInstance},
        {"", "", false, Screen::Industries, false},
        {"APPROVE",   "a", false, Screen::Dashboard,  inInstance},
        {"EVENT",     "e", false, Screen::Dashboard,  inInstance},
        {"UNDO",      "u", false, Screen::Dashboard,  inInstance},
        {"CLOCK",     "t", false, Screen::Dashboard,  inIndustry},
        {"ACT AS",    "p", false, Screen::Dashboard,  inIndustry},
        {"COMMAND",   ":", false, Screen::Dashboard,  true},
        {"HELP",      "?", false, Screen::Dashboard,  true},
        {"QUIT",      "q", false, Screen::Dashboard,  true},
    };
    const int width = getmaxx(w);
    int y = 1;
    for (std::size_t i = 0; i < sizeof(entries) / sizeof(entries[0]) && y < getmaxy(w) - 1; ++i, ++y) {
        const Entry& e = entries[i];
        if (e.label[0] == '\0') {
            putText(w, y, 2, std::string(static_cast<std::size_t>(width - 4), '-'), Tone::Muted, 0);
            continue;
        }
        const bool current = e.screen && e.target == screen_;
        // The current screen pops out as a cyan bar with a slanted end, like the P3R menu.
        const std::string label = std::string(e.label);
        if (current) {
            putText(w, y, 1, fit(" " + label, static_cast<std::size_t>(width - 3)), Tone::Select, 0);
            putGlyphs(w, y, width - 2, "\u25E4", Tone::Gold);
        } else {
            putText(w, y, 3, label, e.enabled ? Tone::Bone : Tone::Muted, 0);
        }
        if (e.key[0] != '\0') {
            putText(w, y, width - 4, e.key, current ? Tone::Select : Tone::Muted, 0);
        }
    }
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawTicker() {
    // Live feed: the newest history entry of the open instance (Observer events land here).
    WINDOW* w = layout_.ticker();
    werase(w);
    putText(w, 0, 0, std::string(static_cast<std::size_t>(COLS), ' '), Tone::Strip, 0);
    putText(w, 0, 1, " LIVE ", Tone::Banner, 0);
    std::string latest = "Choose an industry and an instance to see its events here.";
    if (!controller_.view().instanceId.empty()) {
        std::vector<std::string> lines;
        controller_.history(lines, true, "");
        latest = lines.empty() ? "No events yet." : lines.front();
    }
    putGlyphs(w, 0, 8, "\u25B6", Tone::Strip);
    putText(w, 0, 10, latest, Tone::Strip, 1);
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawPanes() {
    // list
    WINDOW* list = layout_.list();
    werase(list);
    drawFrame(list, listTitle(), focus_ == Pane::List);
    const int width = getmaxx(list) - 4;
    const std::vector<Line> rows = listRows(width);
    const int count = static_cast<int>(rows.size());
    const int visible = std::max(1, getmaxy(list) - 2);
    selected_ = std::max(0, std::min(selected_, count - 1));
    if (selected_ < listTop_) {
        listTop_ = selected_;
    } else if (selected_ >= listTop_ + visible) {
        listTop_ = selected_ - visible + 1;
    }
    listTop_ = std::max(0, std::min(listTop_, std::max(0, count - visible)));

    if (rows.empty()) {
        putText(list, 1, 2, emptyListText(), screen_ == Screen::Industries ? Tone::Red : Tone::Muted);
    }
    for (int row = 0; row < visible && listTop_ + row < count; ++row) {
        const int index = listTop_ + row;
        const bool chosen = index == selected_;
        if (chosen && focus_ != Pane::List) {
            putRow(list, 1 + row, 2, width, rows[index].text, Tone::Gold, false);
            putGlyphs(list, 1 + row, 2, "\u25B7", Tone::Gold);
        } else {
            putRow(list, 1 + row, 2, width, rows[index].text, rows[index].tone, chosen);
        }
    }
    if (listTop_ > 0) {
        putText(list, 0, getmaxx(list) - 10, " more ^ ", Tone::Muted, 1);
    }
    if (listTop_ + visible < count) {
        putText(list, getmaxy(list) - 1, getmaxx(list) - 10, " more v ", Tone::Muted, 1);
    }
    touchwin(list);
    wnoutrefresh(list);

    // detail
    WINDOW* detail = layout_.detail();
    werase(detail);
    std::string title = "DETAIL";
    if (screen_ == Screen::Industries) {
        title = "ABOUT";
    } else if (screen_ == Screen::Instances) {
        title = "INSTANCE";
    } else if (showPanel_) {
        title = panelTitle_;
    }
    const bool panelFocused = focus_ == Pane::Detail || showPanel_;
    drawFrame(detail, title, panelFocused);
    const std::vector<Line> lines = detailLines();
    const int shown = std::max(1, getmaxy(detail) - 2);
    detailTop_ = std::max(0, std::min(detailTop_, static_cast<int>(lines.size()) - shown));
    for (int row = 0; row < shown && detailTop_ + row < static_cast<int>(lines.size()); ++row) {
        putText(detail, 1 + row, 2, lines[detailTop_ + row].text, lines[detailTop_ + row].tone, 2);
    }
    if (detailTop_ + shown < static_cast<int>(lines.size())) {
        putText(detail, getmaxy(detail) - 1, getmaxx(detail) - 22, " more v (Tab, Down) ", Tone::Muted, 1);
    }
    touchwin(detail);
    wnoutrefresh(detail);
}

void TuiFrontEnd::drawBody() {
    WINDOW* w = layout_.body();
    werase(w);
    int firstRow = 1;
    if (screen_ == Screen::History) {
        drawFrame(w, "HISTORY  " + number(static_cast<int>(historyLines_.size())) + " entries, " +
                         (newestFirst_ ? "newest first" : "oldest first") +
                         (filter_.empty() ? "" : ", containing '" + filter_ + "'"),
                  true);
    } else {
        drawFrame(w, "REPORTS", true);
        int x = 2;
        for (int t = 0; t < kReportTabCount; ++t) {
            const bool active = t == reportTab_;
            const std::string label = active ? std::string("[ ") + kReportTabs[t] + " ]"
                                             : std::string("  ") + kReportTabs[t] + "  ";
            putText(w, 1, x, label, active ? Tone::Gold : Tone::Muted, 2);
            x += static_cast<int>(label.size()) + 2;
        }
        firstRow = 3;
    }

    const std::vector<Line> lines = bodyLines();
    const int shown = std::max(1, getmaxy(w) - firstRow - 1);
    const int count = static_cast<int>(lines.size());
    bodyTop_ = std::max(0, std::min(bodyTop_, count - shown));
    for (int row = 0; row < shown && bodyTop_ + row < count; ++row) {
        putText(w, firstRow + row, 2, lines[bodyTop_ + row].text, lines[bodyTop_ + row].tone, 2);
    }
    if (count > shown) {
        char where[48];
        std::snprintf(where, sizeof(where), " lines %d-%d of %d ", bodyTop_ + 1, std::min(count, bodyTop_ + shown),
                      count);
        putText(w, getmaxy(w) - 1, getmaxx(w) - static_cast<int>(std::string(where).size()) - 2, where,
                Tone::Muted, 1);
    }
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawStatus() {
    WINDOW* w = layout_.status();
    werase(w);
    putText(w, 0, 1, hints(), Tone::Muted, 0);

    Tone tone = Tone::Bone;
    std::string prefix;
    if (status_.kind == StatusMessage::Kind::Error) {
        tone = Tone::Red;
        prefix = "! ";
    } else if (status_.kind == StatusMessage::Kind::Success) {
        tone = Tone::Gold;
        prefix = "+ ";
    }
    if (!status_.text.empty()) {
        putText(w, 1, 1, prefix + status_.text, tone, 0);
    }
    touchwin(w);
    wnoutrefresh(w);
}

} // namespace ui
} // namespace tividy
