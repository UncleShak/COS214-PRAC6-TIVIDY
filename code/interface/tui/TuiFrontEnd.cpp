#include "TuiFrontEnd.h"

#include <algorithm>
#include <cctype>
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

/** The route bullet of each action in the Detail pane: its key letter and colour. */
struct ActionBullet { WorkAction action; const char* key; Tone tone; };
const ActionBullet kActionBullets[] = {
    {WorkAction::Start, "S", Tone::BulletLine},     {WorkAction::Complete, "C", Tone::BulletGreen},
    {WorkAction::Reject, "R", Tone::BulletRed},     {WorkAction::Escalate, "E", Tone::Escalated},
    {WorkAction::Assign, "A", Tone::BulletPurple},  {WorkAction::Cancel, "X", Tone::Key},
};
const int kActionBulletCount = 6;

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
    return instance.stalled ? Tone::Red : Tone::Line;
}

bool isFinal(const std::string& state) {
    return state == "Completed" || state == "Rejected" || state == "Cancelled";
}

/** The station glyph of a work item: ● done, ◉ current, ○ upcoming. */
Span station(const WorkItemView& item) {
    const std::string& state = item.state;
    if (state == "Completed") return Span("\u25CF", Tone::Bright);
    if (state == "Rejected")  return Span("\u25CF", Tone::Red);
    if (state == "Cancelled") return Span("\u25CF", Tone::Dim);
    if (item.trackerStatus == "waiting" || state == "Created") return Span("\u25CB", Tone::Muted);
    return Span("\u25C9", Tone::Line);
}

/** The state word as shown at the right of a row: escalation as a yellow block. */
Span stateLabel(const WorkItemView& item) {
    const bool waiting = item.trackerStatus == "waiting";
    const std::string text = waiting ? std::string("Waiting") : item.state;
    const Tone tone = Theme::forState(item.state, item.trackerStatus);
    return Span(tone == Tone::Escalated ? " " + text + " " : text, tone);
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
      newestFirst_(false), reportTab_(0), running_(true), pending_(false), pendingKey_(0),
      pendingAction_(WorkAction::Assign) {
    enter(Screen::Industries);
}

void TuiFrontEnd::run() {
    draw();
    while (running_) {
        const int key = getch();
        if (!pending_ && key != KEY_RESIZE) {
            status_ = StatusMessage::none();   // an announcement lasts until the next key
        }
        if (key == KEY_RESIZE) {
            Log::write("resize");
            layout_.rebuild();
        } else if (layout_.tooSmall()) {
            if (key == 'q' || key == 'Q') {
                running_ = false;
            }
        } else if (pending_) {
            confirmKey(key);
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
    case 'i':
        if (screen_ != Screen::Industries) {
            enter(Screen::Industries);
            return true;
        }
        return false;
    case 'd':
        if (screen_ != Screen::Dashboard && !controller_.view().instanceId.empty()) {
            enter(Screen::Dashboard);
            return true;
        }
        return false;
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
    } else if (key == 'S' || key == 'C' || key == 'R' || key == 'E' || key == 'A' || key == 'X') {
        actionKey(key);
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
    const int page = listWindow() != 0 ? std::max(1, getmaxy(listWindow()) - 2) : 10;
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
    performAction(kActions[pick], item, false);
}

std::string TuiFrontEnd::missingRole(WorkAction action, const WorkItemView& item) const {
    if (item.requiredRole.empty() ||
        (action != WorkAction::Start && action != WorkAction::Complete && action != WorkAction::Reject)) {
        return "";
    }
    const SessionView& view = controller_.view();
    for (std::size_t i = 0; i < view.participants.size(); ++i) {
        if (view.participants[i].id == view.actorId) {
            return hasRole(view.participants[i], item.requiredRole) ? "" : item.requiredRole;
        }
    }
    return "";
}

void TuiFrontEnd::actionKey(int key) {
    const WorkItemView* selected = selectedItem();
    if (selected == 0) {
        status_ = StatusMessage::invalidInput("no work item is selected");
        return;
    }
    WorkAction action = WorkAction::Assign;
    for (int i = 0; i < kActionBulletCount; ++i) {
        if (kActionBullets[i].key[0] == key) {
            action = kActionBullets[i].action;
        }
    }
    std::string verb = actionName(action);
    for (std::size_t i = 0; i < verb.size(); ++i) {
        verb[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(verb[i])));
    }
    const std::string role = missingRole(action, *selected);
    if (!role.empty()) {
        status_ = StatusMessage::engineError("only " + role + " can " + verb + ".");
        return;
    }
    if (action == WorkAction::Reject || action == WorkAction::Cancel) {
        pending_ = true;
        pendingKey_ = key;
        pendingAction_ = action;
        pendingItem_ = selected->id;
        return;
    }
    performAction(action, *selected, false);
}

void TuiFrontEnd::confirmKey(int key) {
    pending_ = false;
    const WorkItemView* item = findItem(pendingItem_);
    if (key != pendingKey_ || item == 0) {
        status_ = StatusMessage::info(std::string(actionName(pendingAction_)) + " not confirmed, nothing changed");
        return;
    }
    const WorkItemView copy = *item;
    performAction(pendingAction_, copy, true);
}

void TuiFrontEnd::performAction(WorkAction action, const WorkItemView& item, bool confirmed) {
    std::string assignee;
    std::string reason;
    if (action == WorkAction::Assign) {
        draw();
        const std::vector<ParticipantView>& people = controller_.view().participants;
        std::vector<Line> choices;
        choices.push_back(Line("Auto (the industry's assignment rule chooses)", Tone::Line));
        for (std::size_t i = 0; i < people.size(); ++i) {
            const bool fits = item.requiredRole.empty() || hasRole(people[i], item.requiredRole);
            choices.push_back(Line(describePerson(people[i]), fits ? Tone::Text : Tone::Muted));
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
    } else if (action == WorkAction::Cancel && !confirmed) {
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
        entries.push_back(Line(text, a.status == "Pending" ? Tone::Line : Tone::Muted));
    }
    const int pick = Dialog::menu("APPROVALS", entries);
    if (pick == Dialog::kCancelled) {
        return;
    }
    const ApprovalView approval = approvals[pick];
    const Tone tone = approval.status == "Pending" ? Tone::Text : Tone::Muted;

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
    lines.push_back(Line(status_.text, Tone::Line));
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
        lines.push_back(Line((cancelled ? "x  " : "+  ") + changes[i], cancelled ? Tone::Red : Tone::Line));
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
    lines.push_back(Line("Everywhere", Tone::Line));
    lines.push_back(Line("  Up/Down or j/k   move              PgUp/PgDn, Home/End   jump"));
    lines.push_back(Line("  Enter            open or choose    b, Esc, Left          back"));
    lines.push_back(Line("  :                type a command (the same commands as tividy --plain)"));
    lines.push_back(Line("  ?                this help         q                     quit (asks first)"));
    lines.push_back(Line("  Ctrl+L           redraw            Ctrl+C                quit at once"));
    lines.push_back(Line("  i                industries        d                     dashboard"));
    lines.push_back(Line());

    switch (screen_) {
    case Screen::Industries:
        lines.push_back(Line("Industries", Tone::Line));
        lines.push_back(Line("  Enter            work in the selected industry"));
        break;
    case Screen::Instances:
        lines.push_back(Line("Instances", Tone::Line));
        lines.push_back(Line("  Enter            open the selected instance"));
        lines.push_back(Line("  n                start a new instance (asks for its start data)"));
        lines.push_back(Line("  p                choose who you are acting as"));
        lines.push_back(Line("  t                move the engine clock forward"));
        lines.push_back(Line("  i                change industry"));
        break;
    case Screen::Dashboard:
        lines.push_back(Line("Instance dashboard", Tone::Line));
        lines.push_back(Line("  Enter            actions on the selected work item"));
        lines.push_back(Line("  S C R E A X      start, complete, reject, escalate, assign, cancel"));
        lines.push_back(Line("                   (R and X ask you to press the key again to confirm)"));
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
        lines.push_back(Line("Colours: line colour = started, purple = assigned, green = complete,", Tone::Muted));
        lines.push_back(Line("yellow block = escalated, red = rejected or an error, grey = not reached.", Tone::Muted));
        break;
    case Screen::History:
        lines.push_back(Line("History", Tone::Line));
        lines.push_back(Line("  o                oldest first / newest first"));
        lines.push_back(Line("  /                show only entries containing some text"));
        lines.push_back(Line("  Esc              clear the filter (or go back if there is none)"));
        break;
    case Screen::Reports:
        lines.push_back(Line("Reports", Tone::Line));
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
    panel_.push_back(Line(outcome.commandDescription, Tone::Line));
    panel_.push_back(Line());
    if (!outcome.previousState.empty() || !outcome.newState.empty()) {
        panel_.push_back(Line("State:          " + orDash(outcome.previousState) + " -> " + orDash(outcome.newState),
                              Theme::forState(outcome.newState, "")));
    }
    if (!outcome.routingDescription.empty()) {
        panel_.push_back(Line("Routing:        " + outcome.routingDescription, Tone::Line));
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
        panel_.push_back(Line("The instance is complete.", Tone::Line));
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
    const std::vector<int> order = itemOrder();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (rows[order[i]].id == itemId) {
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

std::vector<int> TuiFrontEnd::itemOrder() const {
    const std::vector<WorkItemView>& rows = items();
    std::vector<int> order;
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (isException(rows[i].id) == (pass == 1)) {
                order.push_back(static_cast<int>(i));
            }
        }
    }
    return order;
}

const WorkItemView* TuiFrontEnd::selectedItem() const {
    const std::vector<WorkItemView>& rows = items();
    const std::vector<int> order = itemOrder();
    if (selected_ < 0 || selected_ >= static_cast<int>(order.size())) {
        return 0;
    }
    return &rows[order[selected_]];
}

const std::vector<RouteStop>& TuiFrontEnd::routeMapFor(const std::string& industryId,
                                                       const std::string& definition) const {
    const std::string key = industryId + "/" + definition;
    std::map<std::string, std::vector<RouteStop> >::iterator it = routeMaps_.find(key);
    if (it == routeMaps_.end()) {
        it = routeMaps_.insert(std::make_pair(key, controller_.routeMap(industryId, definition))).first;
    }
    return it->second;
}

RouteModel TuiFrontEnd::currentRoute() const {
    const SessionView& view = controller_.view();
    return RouteModel::build(view.workItems, routeMapFor(view.industryId, view.instance.definitionName));
}

bool TuiFrontEnd::isException(const std::string& itemId) const {
    const SessionView& view = controller_.view();
    if (view.instanceId.empty()) {
        return false;
    }
    const std::vector<RouteStop>& map = routeMapFor(view.industryId, view.instance.definitionName);
    for (std::size_t i = 0; i < map.size(); ++i) {
        if (map[i].workItemId == itemId) {
            return map[i].exception;
        }
    }
    return false;
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

std::vector<Line> TuiFrontEnd::listRows(int width, std::vector<int>& rowIndex) const {
    const SessionView& view = controller_.view();
    std::vector<Line> rows;
    rowIndex.clear();
    if (screen_ == Screen::Industries) {
        for (std::size_t i = 0; i < view.industries.size(); ++i) {
            Line row;
            row.add(" ", Tone::Text).add(bullet(number(static_cast<int>(i) + 1)), Theme::industryBullet(static_cast<int>(i)));
            row.add("  " + view.industries[i].displayName, Tone::TextBold);
            rows.push_back(row);
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
        const std::vector<int> order = itemOrder();
        for (std::size_t n = 0; n < order.size(); ++n) {
            const WorkItemView& item = work[order[n]];
            const bool exception = isException(item.id);
            if (exception && (n == 0 || !isException(work[order[n - 1]].id))) {
                if (!rows.empty()) {
                    rows.push_back(Line());
                    rowIndex.push_back(-1);
                }
                rows.push_back(Line(" EXCEPTION ROUTE", Tone::Red));
                rowIndex.push_back(-1);
            }
            Span glyph = station(item);
            if (exception && glyph.text == "\u25CB") {
                glyph.tone = Tone::Red;
            }
            const Span state = stateLabel(item);
            Line row;
            row.add(" ", Tone::Text).add(glyph.text, glyph.tone).add("  " + item.name, Tone::Text);
            row.addRight(state.text, state.tone).addRight(" ", Tone::Text);
            rows.push_back(row);
            rowIndex.push_back(static_cast<int>(n));
        }
        (void)width;
        return rows;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rowIndex.push_back(static_cast<int>(i));
    }
    return rows;
}

std::vector<Line> TuiFrontEnd::detailLines() const {
    const SessionView& view = controller_.view();
    std::vector<Line> lines;

    if (screen_ == Screen::Industries) {
        if (selected_ < static_cast<int>(view.industries.size())) {
            const IndustryInfo& industry = view.industries[selected_];
            lines.push_back(Line(industry.displayName, Tone::Line));
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
            lines.push_back(Line(instance.id, Tone::Line));
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
            lines.push_back(Line("The instance is complete.", Tone::Line));
        } else if (view.instance.stalled) {
            lines.push_back(Line("The instance is stalled: nothing can move forward.", Tone::Red));
        }
        if (availableOnly_) {
            lines.push_back(Line());
            lines.push_back(Line("v: show all work items", Tone::Muted));
        }
        return lines;
    }

    lines.push_back(Line(item->name, Tone::Line));
    lines.push_back(Line());
    lines.push_back(Line("State:        " + item->state, Theme::forState(item->state, item->trackerStatus)));
    lines.push_back(Line("Progress:     " + (item->trackerStatus.empty() ? std::string("not reached yet")
                                                                         : item->trackerStatus)));
    lines.push_back(Line("Role:         " + orDash(item->requiredRole)));
    lines.push_back(Line("Assignee:     " + orDash(item->assigneeName)));
    lines.push_back(Line("Waiting on:   " + orDash(join(item->waitingOn, ", ")),
                         item->waitingOn.empty() ? Tone::Text : Tone::Muted));
    if (item->dueAt >= 0) {
        const bool overdue = view.clock > item->dueAt && !isFinal(item->state);
        lines.push_back(Line("Due:          t=" + number(item->dueAt) + (overdue ? "  OVERDUE" : ""),
                             overdue ? Tone::Red : Tone::Text));
    } else {
        lines.push_back(Line("Due:          -"));
    }

    lines.push_back(Line());
    lines.push_back(Line("ALLOWED NOW", Tone::Muted));
    const std::vector<WorkAction> hinted = InterfaceController::hintedActions(item->state);
    Line row;
    int inRow = 0;
    for (int i = 0; i < kActionBulletCount; ++i) {
        if (std::find(hinted.begin(), hinted.end(), kActionBullets[i].action) == hinted.end()) {
            continue;
        }
        const bool barred = !missingRole(kActionBullets[i].action, *item).empty();
        row.add(bullet(kActionBullets[i].key), barred ? Tone::Dim : kActionBullets[i].tone);
        row.add(" " + fit(actionName(kActionBullets[i].action), 11), barred ? Tone::Dim : Tone::Text);
        if (++inRow == 2) {
            lines.push_back(row);
            row = Line();
            inRow = 0;
        }
    }
    if (inRow > 0) {
        lines.push_back(row);
    }
    if (hinted.empty()) {
        lines.push_back(Line(isFinal(item->state) ? "Nothing: this stop is final." : "Nothing until it becomes available.",
                             Tone::Muted));
    }

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
                             a.status == "Pending" ? Tone::Line : (a.status == "Rejected" ? Tone::Red : Tone::Muted)));
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
    return availableOnly_ ? "AVAILABLE WORK" : "WORK ITEMS";
}

std::string TuiFrontEnd::listSummary() const {
    if (screen_ != Screen::Dashboard) {
        return "";
    }
    const SessionView& view = controller_.view();
    int done = 0;
    for (std::size_t i = 0; i < view.workItems.size(); ++i) {
        if (view.workItems[i].state == "Completed") {
            ++done;
        }
    }
    return number(done) + " of " + number(static_cast<int>(view.workItems.size())) + " done";
}

std::string TuiFrontEnd::emptyListText() const {
    switch (screen_) {
    case Screen::Industries: return "No industries registered with the engine.";
    case Screen::Instances:  return "No instances yet. Press n to start one.";
    default:                 return availableOnly_ ? "No work available." : "No work items.";
    }
}

std::vector<std::string> TuiFrontEnd::breadcrumbs() const {
    const SessionView& view = controller_.view();
    std::vector<std::string> path;
    if (screen_ != Screen::Industries) {
        path.push_back(industryName());
    }
    if (screen_ == Screen::Dashboard || screen_ == Screen::History || screen_ == Screen::Reports) {
        path.push_back(view.instanceId);
    }
    if (screen_ == Screen::History) {
        path.push_back("History");
    } else if (screen_ == Screen::Reports) {
        path.push_back("Reports");
    }
    return path;
}

std::vector<std::pair<std::string, std::string> > TuiFrontEnd::hints() const {
    typedef std::pair<std::string, std::string> Hint;
    std::vector<Hint> out;
    const std::string enter = "\u23CE";
    switch (screen_) {
    case Screen::Industries:
        out.push_back(Hint(enter, "open"));
        break;
    case Screen::Instances:
        out.push_back(Hint(enter, "open"));
        out.push_back(Hint("n", "new train"));
        out.push_back(Hint("p", "act as"));
        out.push_back(Hint("t", "clock"));
        out.push_back(Hint("b", "back"));
        break;
    case Screen::Dashboard:
        if (showPanel_) {
            out.push_back(Hint(enter, "back to detail"));
            break;
        }
        out.push_back(Hint(enter, "actions"));
        out.push_back(Hint("v", "available/all"));
        out.push_back(Hint("Tab", "detail"));
        out.push_back(Hint("b", "back"));
        break;
    case Screen::History:
        out.push_back(Hint("o", "oldest/newest"));
        out.push_back(Hint("/", "filter"));
        out.push_back(Hint("Esc", "clear filter"));
        out.push_back(Hint("b", "back"));
        break;
    case Screen::Reports:
        out.push_back(Hint("\u2190\u2192", "switch report"));
        out.push_back(Hint("r", "reload"));
        out.push_back(Hint("b", "back"));
        break;
    }
    out.push_back(Hint(":", "command"));
    out.push_back(Hint("?", "help"));
    out.push_back(Hint("q", "quit"));
    return out;
}

// ---- drawing ----------------------------------------------------------------------------

int TuiFrontEnd::lineIndustry() const {
    const SessionView& view = controller_.view();
    if (screen_ == Screen::Industries) {
        return selected_;
    }
    for (std::size_t i = 0; i < view.industries.size(); ++i) {
        if (view.industries[i].id == view.industryId) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void TuiFrontEnd::draw() {
    const int industry = lineIndustry();
    if (industry != Theme::industry()) {
        Theme::applyTheme(industry);
    }
    if (layout_.tooSmall()) {
        drawTooSmall();
        return;
    }
    touchwin(stdscr);   // the blank rows between panes: wipe what a closed dialog left there
    wnoutrefresh(stdscr);
    drawHeader();
    drawMenu();
    if (screen_ == Screen::Dashboard) {
        drawRoute();
        drawPanes();
    } else if (screen_ == Screen::History || screen_ == Screen::Reports) {
        drawBody();
    } else {
        drawPanes();
    }
    drawAnnounce();
    drawFooter();
    doupdate();
}

WINDOW* TuiFrontEnd::listWindow() const {
    return screen_ == Screen::Dashboard ? layout_.list() : layout_.pageList();
}

WINDOW* TuiFrontEnd::detailWindow() const {
    return screen_ == Screen::Dashboard ? layout_.detail() : layout_.pageDetail();
}

void TuiFrontEnd::drawTooSmall() {
    Theme::paint(stdscr);
    erase();
    char message[64];
    std::snprintf(message, sizeof(message), "Make the window at least %d \u00D7 %d", Layout::kMinCols,
                  Layout::kMinRows);
    const int width = static_cast<int>(cells(message));
    putGlyphs(stdscr, LINES / 2, std::max(0, (COLS - width) / 2), message, Tone::Text);
    wnoutrefresh(stdscr);
    doupdate();
}

void TuiFrontEnd::drawHeader() {
    const SessionView& view = controller_.view();
    WINDOW* w = layout_.header();
    werase(w);
    fillRow(w, 0, 0, COLS, Tone::Band);
    fillRow(w, 1, 0, COLS, Tone::Band);

    // Row 1, right: every industry's route bullet and name; the active one bright.
    const int active = lineIndustry();
    int right = COLS - 1;
    for (int i = static_cast<int>(view.industries.size()) - 1; i >= 0; --i) {
        const std::string name = view.industries[i].displayName;
        right -= static_cast<int>(name.size());
        putText(w, 0, right, name, i == active ? Tone::BandBright : Tone::BandMuted, 0);
        right -= 4;
        putText(w, 0, right, " " + number(i + 1) + " ", i == active ? Theme::industryBullet(i) : Tone::KeyMuted, 0);
        right -= 3;
    }

    // Row 1, left: the active line's bullet, TIVIDY, then the breadcrumbs.
    putText(w, 0, 1, " " + number(active + 1) + " ", Theme::industryBullet(active), 0);
    putText(w, 0, 5, "TIVIDY", Tone::BandBright, 0);
    int x = 11;
    const std::vector<std::string> crumbs = breadcrumbs();
    for (std::size_t i = 0; i < crumbs.size() && x < right - 4; ++i) {
        putGlyphs(w, 0, x + 1, "\u203A", Tone::BandMuted);
        putText(w, 0, x + 3, crumbs[i], Tone::Band, COLS - right + 2);
        x += 3 + static_cast<int>(cells(crumbs[i]));
    }

    // Row 2: context in grey, the engine label (and what undo would reverse) on the right.
    std::string context = "t=" + number(view.clock);
    if (!view.instanceId.empty()) {
        context += "   \u00B7   " + view.instanceId + " " + instanceStatus(view.instance);
    } else if (!view.industryId.empty()) {
        context += "   \u00B7   " + industryName();
    }
    const std::string actor = controller_.participantName(view.actorId);
    if (!actor.empty()) {
        std::string roles;
        for (std::size_t i = 0; i < view.participants.size(); ++i) {
            if (view.participants[i].id == view.actorId) {
                roles = join(view.participants[i].roles, ", ");
            }
        }
        context += "   \u00B7   acting as " + actor + (roles.empty() ? "" : " (" + roles + ")");
    }
    const std::string next = screen_ == Screen::Dashboard ? controller_.nextUndo() : "";
    const std::string label = (next.empty() ? "" : "u undo " + next + "   ") + "[" + engineLabel_ + "]";
    const int labelX = std::max(COLS / 2, COLS - 1 - static_cast<int>(label.size()));
    putText(w, 1, 1, cutCells(context, static_cast<std::size_t>(labelX - 3)), Tone::BandMuted, 0);
    putText(w, 1, labelX, label, Tone::BandMuted, 1);
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawRoute() {
    const SessionView& view = controller_.view();
    WINDOW* w = layout_.route();
    werase(w);
    const RouteModel route = currentRoute();
    drawFrame(w, "ROUTE \u00B7 " + view.instanceId, false,
              number(route.done) + " of " + number(static_cast<int>(route.main.size())) + " stops");
    ui::drawRoute(w, route);
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawMenu() {
    WINDOW* w = screen_ == Screen::Dashboard ? layout_.menu() : layout_.pageMenu();
    werase(w);
    drawFrame(w, "MENU", false);
    const bool inInstance = !controller_.view().instanceId.empty();
    const bool inIndustry = !controller_.view().industryId.empty();
    struct Entry { const char* label; const char* key; bool screen; Screen target; bool enabled; };
    const Entry entries[] = {
        {"Industry",  "i", true,  Screen::Industries, true},
        {"Instances", "b", true,  Screen::Instances,  inIndustry},
        {"Dashboard", "d", true,  Screen::Dashboard,  inInstance},
        {"History",   "h", true,  Screen::History,    inInstance},
        {"Reports",   "r", true,  Screen::Reports,    inInstance},
        {"", "", false, Screen::Industries, false},
        {"Approve",   "a", false, Screen::Dashboard,  inInstance},
        {"Event",     "e", false, Screen::Dashboard,  inInstance},
        {"Undo",      "u", false, Screen::Dashboard,  inInstance},
        {"Clock",     "t", false, Screen::Dashboard,  inIndustry},
        {"Act as",    "p", false, Screen::Dashboard,  inIndustry},
        {"Command",   ":", false, Screen::Dashboard,  true},
        {"Help",      "?", false, Screen::Dashboard,  true},
        {"Quit",      "q", false, Screen::Dashboard,  true},
    };
    const int inner = getmaxx(w) - 2;
    int y = 1;
    for (std::size_t i = 0; i < sizeof(entries) / sizeof(entries[0]) && y < getmaxy(w) - 1; ++i, ++y) {
        const Entry& e = entries[i];
        if (e.label[0] == '\0') {
            std::string rule;
            for (int c = 0; c < inner - 2; ++c) {
                rule += "\u2500";
            }
            putText(w, y, 2, rule, Tone::Dim, 0);
            continue;
        }
        const bool current = e.screen && e.target == screen_;
        Line row;
        row.add(bullet(e.key), e.enabled ? Tone::Key : Tone::Dim);
        row.add(" " + std::string(e.label), current ? Tone::Select : (e.enabled ? Tone::Text : Tone::Dim));
        if (current) {
            fillRow(w, y, 1, inner, Tone::Select);
        }
        putLine(w, y, 2, inner - 2, row, current);
    }
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawAnnounce() {
    const SessionView& view = controller_.view();
    WINDOW* w = layout_.announce();
    werase(w);
    fillRow(w, 0, 0, COLS, Tone::Band);
    Line line;
    bool alarm = false;
    if (pending_) {
        const WorkItemView* item = findItem(pendingItem_);
        line.add(std::string(actionName(pendingAction_)) + " ", Tone::Band)
            .add(item != 0 ? item->name : pendingItem_, Tone::BandBright)
            .add("? Press " + std::string(1, static_cast<char>(pendingKey_)) + " again to confirm.", Tone::Band);
    } else if (status_.isError()) {
        alarm = true;
        line.add("Stand clear:", Tone::BandRed).add(" " + status_.text, Tone::Band);
    } else if (!status_.text.empty()) {
        line.add(status_.text, Tone::Band);
    } else if (view.industryId.empty()) {
        line.add("Choose a line to see its trains.", Tone::Band);
    } else if (view.instanceId.empty()) {
        line.add("Choose a train, or press n to start one.", Tone::Band);
    } else {
        const RouteModel route = currentRoute();
        const Station* now = route.lastReached();
        const Station* next = route.nextStation();
        if (now == 0) {
            line.add("This train has not left yet.", Tone::Band);
        } else {
            line.add("This is ", Tone::Band).add(now->name, Tone::BandBright);
            if (next != 0) {
                line.add(". Next stop is ", Tone::Band).add(next->name, Tone::BandBright).add(".", Tone::Band);
            } else {
                line.add(". Last stop.", Tone::Band);
            }
        }
    }
    putGlyphs(w, 0, 1, "\u25B6", alarm ? Tone::BandRed : Tone::BandBright);
    const std::string clock = "t=" + number(view.clock);
    putLine(w, 0, 3, COLS - 6 - static_cast<int>(clock.size()), line, false);
    putText(w, 0, COLS - 1 - static_cast<int>(clock.size()), clock, Tone::BandMuted, 0);
    touchwin(w);
    wnoutrefresh(w);
}

void TuiFrontEnd::drawPanes() {
    // list
    WINDOW* list = listWindow();
    werase(list);
    drawFrame(list, listTitle(), focus_ == Pane::List, listSummary());
    const int width = getmaxx(list) - 4;
    std::vector<int> rowIndex;
    const std::vector<Line> rows = listRows(width, rowIndex);
    const int count = static_cast<int>(rows.size());
    const int visible = std::max(1, getmaxy(list) - 2);
    selected_ = std::max(0, std::min(selected_, rowCount() - 1));
    int chosenRow = 0;
    for (int r = 0; r < count; ++r) {
        if (rowIndex[r] == selected_) {
            chosenRow = r;
        }
    }
    if (chosenRow < listTop_) {
        listTop_ = chosenRow > 0 && rowIndex[chosenRow - 1] < 0 ? chosenRow - 1 : chosenRow;
    } else if (chosenRow >= listTop_ + visible) {
        listTop_ = chosenRow - visible + 1;
    }
    listTop_ = std::max(0, std::min(listTop_, std::max(0, count - visible)));

    if (rows.empty()) {
        putText(list, 1, 2, emptyListText(), Tone::Muted);
    }
    for (int row = 0; row < visible && listTop_ + row < count; ++row) {
        const int r = listTop_ + row;
        const bool chosen = rowIndex[r] >= 0 && r == chosenRow;
        if (chosen && focus_ != Pane::List) {
            fillRow(list, 1 + row, 1, getmaxx(list) - 2, Tone::Band);
            putLine(list, 1 + row, 2, width, rows[r], false);
        } else {
            if (chosen) {
                fillRow(list, 1 + row, 1, getmaxx(list) - 2, Tone::Select);
            }
            putLine(list, 1 + row, 2, width, rows[r], chosen);
        }
    }
    if (listTop_ > 0 || listTop_ + visible < count) {
        const std::string more = listTop_ > 0 && listTop_ + visible < count ? " more \u2191\u2193 "
                                 : (listTop_ > 0 ? " more \u2191 " : " more \u2193 ");
        putText(list, getmaxy(list) - 1, getmaxx(list) - 3 - static_cast<int>(cells(more)), more, Tone::Muted, 1);
    }
    touchwin(list);
    wnoutrefresh(list);

    // detail
    WINDOW* detail = detailWindow();
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
        putLine(detail, 1 + row, 2, getmaxx(detail) - 4, lines[detailTop_ + row], false);
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
            putText(w, 1, x, label, active ? Tone::Line : Tone::Muted, 2);
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

void TuiFrontEnd::drawFooter() {
    WINDOW* w = layout_.footer();
    werase(w);
    const std::vector<std::pair<std::string, std::string> > keys = hints();
    Line row;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        row.add(bullet(keys[i].first), Tone::Key).add(" " + keys[i].second + "   ", Tone::Muted);
    }
    putLine(w, 0, 1, COLS - 2, row, false);
    touchwin(w);
    wnoutrefresh(w);
}

} // namespace ui
} // namespace tividy
