#include "LineInterpreter.h"

#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>

#include "InputValidation.h"

namespace tividy {
namespace ui {

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

std::string lower(const std::string& text) {
    std::string out(text);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

std::string joinWords(const std::vector<std::string>& words, std::size_t from, std::size_t to) {
    std::string out;
    for (std::size_t i = from; i < to && i < words.size(); ++i) {
        out += (i == from ? "" : " ") + words[i];
    }
    return out;
}

std::string joinList(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        out += (i == 0 ? "" : ", ") + values[i];
    }
    return out;
}

std::string pad(const std::string& text, std::size_t width) {
    return text.size() >= width ? text + " " : text + std::string(width - text.size(), ' ');
}

std::string number(std::size_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    return text;
}

std::string numberInt(int value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

bool allDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    return true;
}

Reply invalid(const std::string& text) {
    Reply reply;
    reply.status = StatusMessage::invalidInput(text);
    return reply;
}

Reply withStatus(const StatusMessage& status) {
    Reply reply;
    reply.status = status;
    return reply;
}

} // namespace

LineInterpreter::LineInterpreter(InterfaceController& controller) : controller_(controller) {}

Reply LineInterpreter::run(const std::string& line) {
    std::vector<std::string> words = split(line);
    if (words.empty()) {
        return Reply();
    }
    const std::string command = lower(words[0]);
    std::vector<std::string> args(words.begin() + 1, words.end());

    if (command == "help" || command == "?") {
        Reply reply;
        reply.lines = helpText();
        return reply;
    }
    if (command == "quit" || command == "exit" || command == "q") {
        Reply reply;
        reply.quit = true;
        return reply;
    }
    if (command == "industries") return listIndustries();
    if (command == "industry") return chooseIndustry(args);
    if (command == "instances") return listInstances();
    if (command == "open") return openInstance(args);
    if (command == "start") return start(args);
    if (command == "people") return listPeople();
    if (command == "as") return actAs(args);
    if (command == "items") return listItems(false);
    if (command == "work") return listItems(true);
    if (command == "show") return showItem(args);
    if (command == "assign") return workAction(WorkAction::Assign, args);
    if (command == "begin") return workAction(WorkAction::Start, args);
    if (command == "complete") return workAction(WorkAction::Complete, args);
    if (command == "reject-work") return workAction(WorkAction::Reject, args);
    if (command == "cancel") return workAction(WorkAction::Cancel, args);
    if (command == "escalate") return workAction(WorkAction::Escalate, args);
    if (command == "approvals") return listApprovals();
    if (command == "approve") return decideApproval(true, args);
    if (command == "reject") return decideApproval(false, args);
    if (command == "advance") return advance(args);
    if (command == "events") return listEvents();
    if (command == "event") return raiseEvent(args);
    if (command == "undo") return undo();
    if (command == "history") return history(args);
    if (command == "report") return report();

    return invalid("unknown command '" + words[0] + "'. Type 'help' for the list.");
}

std::string LineInterpreter::prompt() const {
    const SessionView& view = controller_.view();
    std::string text = "tividy";
    if (!view.industryId.empty()) {
        text += " " + view.industryId;
    }
    if (!view.instanceId.empty()) {
        text += "/" + view.instanceId;
    }
    if (!view.actorId.empty()) {
        text += " (" + controller_.participantName(view.actorId) + ")";
    }
    return text + "> ";
}

std::vector<std::string> LineInterpreter::helpText() {
    const char* lines[] = {
        "Getting started",
        "  industries                     list industries",
        "  industry <n|id>                work in an industry",
        "  start key=value ...            start an instance (type 'start' to see the fields)",
        "  instances / open <n|id>        list instances / switch to one",
        "Work",
        "  items / work                   all work items / available work only",
        "  show <item>                    details of one item",
        "  people / as <person>           list participants / act as someone",
        "  assign <item> [to <person>]    assign (without 'to', the assignment rule chooses)",
        "  begin <item>                   start working on an item",
        "  complete <item>                finish an item (routing decides what comes next)",
        "  reject-work <item> <reason>    reject a work item",
        "  cancel <item> / escalate <item>",
        "Approvals and time",
        "  approvals                      list approval requests",
        "  approve <id>                   approve a request, e.g. approve APR-1",
        "  reject <id> <reason>           reject a request",
        "  advance <units>                move the clock forward (deadlines)",
        "Events and undo",
        "  events                         events this industry reacts to",
        "  event <TYPE> [what happened]   raise one, e.g. event WORM_SIGN thumper at 2 km",
        "  undo                           reverse the last action in this instance",
        "Looking back",
        "  history [newest] [text]        history, optionally newest first or filtered",
        "  report                         progress, audit and overdue reports",
        "  help / quit",
        "<item> is a number from 'items' or the item's name; <person> is a number from",
        "'people', an ID or a name.",
    };
    return std::vector<std::string>(lines, lines + sizeof(lines) / sizeof(lines[0]));
}

// ---- industries and instances ---------------------------------------------------------

Reply LineInterpreter::listIndustries() const {
    Reply reply;
    const SessionView& view = controller_.view();
    for (std::size_t i = 0; i < view.industries.size(); ++i) {
        const IndustryInfo& industry = view.industries[i];
        std::string marker = industry.id == view.industryId ? "* " : "  ";
        reply.lines.push_back(marker + number(i + 1) + ". " + pad(industry.displayName, 22) + pad("[" + industry.id + "]", 12) +
                              industry.description);
    }
    if (reply.lines.empty()) {
        reply.status = StatusMessage::info("No industries are registered with the engine");
    } else if (view.industryId.empty()) {
        reply.status = StatusMessage::info("Choose one with: industry <number>");
    }
    return reply;
}

Reply LineInterpreter::chooseIndustry(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return invalid("usage: industry <number or id>");
    }
    const std::vector<IndustryInfo>& industries = controller_.view().industries;
    std::string id = args[0];
    std::size_t index = 0;
    if (allDigits(args[0])) {
        if (!parseMenuChoice(args[0], industries.size(), index)) {
            return invalid("there is no industry " + args[0]);
        }
        id = industries[index].id;
    }
    StatusMessage status = controller_.selectIndustry(id);
    if (status.isError()) {
        return withStatus(status);
    }
    for (std::size_t i = 0; i < industries.size(); ++i) {
        if (industries[i].id == id) {
            Reply reply = listInstances();
            reply.lines.insert(reply.lines.begin(), "Industry: " + industries[i].displayName + ". Definitions: " +
                                                        joinList(industries[i].definitions));
            reply.status = StatusMessage::info(controller_.view().instances.empty()
                                                   ? "No instances yet. Type 'start' to begin one."
                                                   : "Open one with: open <number>, or type 'start'");
            return reply;
        }
    }
    return Reply();
}

Reply LineInterpreter::listInstances() const {
    Reply reply;
    const SessionView& view = controller_.view();
    if (view.industryId.empty()) {
        return invalid("choose an industry first (type 'industries')");
    }
    for (std::size_t i = 0; i < view.instances.size(); ++i) {
        const InstanceView& inst = view.instances[i];
        std::string marker = inst.id == view.instanceId ? "* " : "  ";
        std::string state = inst.complete ? "complete" : (inst.stalled ? "stalled" : "running");
        reply.lines.push_back(marker + number(i + 1) + ". " + pad(inst.id, 12) +
                              pad(inst.definitionName + " v" + inst.definitionVersion, 26) + state);
    }
    if (reply.lines.empty()) {
        reply.status = StatusMessage::info("No instances yet. Type 'start' to begin one.");
    }
    return reply;
}

Reply LineInterpreter::openInstance(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return invalid("usage: open <number or instance id>");
    }
    const std::vector<InstanceView>& instances = controller_.view().instances;
    std::string id = args[0];
    std::size_t index = 0;
    if (allDigits(args[0])) {
        if (!parseMenuChoice(args[0], instances.size(), index)) {
            return invalid("there is no instance " + args[0]);
        }
        id = instances[index].id;
    }
    StatusMessage status = controller_.selectInstance(id);
    if (status.isError()) {
        return withStatus(status);
    }
    Reply reply = listItems(false);
    reply.status = StatusMessage::info("Opened " + id);
    return reply;
}

Reply LineInterpreter::start(const std::vector<std::string>& args) {
    const SessionView& view = controller_.view();
    const IndustryInfo* industry = 0;
    for (std::size_t i = 0; i < view.industries.size(); ++i) {
        if (view.industries[i].id == view.industryId) {
            industry = &view.industries[i];
        }
    }
    if (industry == 0) {
        return invalid("choose an industry first (type 'industries')");
    }
    if (industry->definitions.empty()) {
        return invalid("this industry has no workflow definitions");
    }

    // Words before the first key=value name the definition; default is the first one.
    std::size_t firstPair = 0;
    while (firstPair < args.size() && args[firstPair].find('=') == std::string::npos) {
        ++firstPair;
    }
    std::string definition = firstPair == 0 ? industry->definitions.front() : joinWords(args, 0, firstPair);

    std::vector<DataField> fields;
    StatusMessage status = controller_.startFields(definition, fields);
    if (status.isError()) {
        return withStatus(status);
    }
    std::string usage = "start";
    for (std::size_t i = 0; i < fields.size(); ++i) {
        std::string pair = fields[i].key + "=<" + (fields[i].numeric ? "number" : "text") + ">";
        usage += " " + (fields[i].required ? pair : "[" + pair + "]");
    }

    // key=value; a value runs on over the following words until the next key=value.
    std::map<std::string, std::string> values;
    std::string currentKey;
    for (std::size_t i = firstPair; i < args.size(); ++i) {
        std::size_t equals = args[i].find('=');
        if (equals != std::string::npos && equals > 0) {
            // Field names are matched without case; values keep the field's own spelling.
            const std::string typedKey = args[i].substr(0, equals);
            currentKey.clear();
            for (std::size_t f = 0; f < fields.size(); ++f) {
                if (lower(fields[f].key) == lower(typedKey)) {
                    currentKey = fields[f].key;
                }
            }
            if (currentKey.empty()) {
                Reply reply = invalid("unknown field '" + typedKey + "'");
                reply.lines.push_back("Usage: " + usage);
                return reply;
            }
            values[currentKey] = args[i].substr(equals + 1);
        } else if (!currentKey.empty()) {
            values[currentKey] += " " + args[i];
        }
    }

    if (args.empty()) {
        Reply reply;
        reply.lines.push_back("Starting '" + definition + "' needs:");
        for (std::size_t i = 0; i < fields.size(); ++i) {
            reply.lines.push_back("  " + pad(fields[i].key, 14) + fields[i].label +
                                  (fields[i].required ? "" : " (optional, default '" + fields[i].defaultValue + "')"));
        }
        reply.lines.push_back("Usage: " + usage);
        return reply;
    }

    status = controller_.startInstance(definition, values);
    if (status.isError()) {
        Reply reply = withStatus(status);
        reply.lines.push_back("Usage: " + usage);
        return reply;
    }
    Reply reply = listItems(false);
    reply.status = status;
    return reply;
}

// ---- participants ---------------------------------------------------------------------

Reply LineInterpreter::listPeople() const {
    Reply reply;
    const SessionView& view = controller_.view();
    if (view.industryId.empty()) {
        return invalid("choose an industry first (type 'industries')");
    }
    for (std::size_t i = 0; i < view.participants.size(); ++i) {
        const ParticipantView& p = view.participants[i];
        std::string marker = p.id == view.actorId ? "* " : "  ";
        reply.lines.push_back(marker + number(i + 1) + ". " + pad(p.name, 10) + pad("[" + p.id + "]", 9) + "roles: " +
                              pad(joinList(p.roles), 22) + "workload " + numberInt(p.workload));
    }
    reply.status = StatusMessage::info("* marks who is acting. Change with: as <person>");
    return reply;
}

Reply LineInterpreter::actAs(const std::vector<std::string>& args) {
    if (args.empty()) {
        return invalid("usage: as <number, id or name>");
    }
    std::string id = resolvePerson(args);
    if (id.empty()) {
        return invalid("no participant called '" + joinWords(args, 0, args.size()) + "' (type 'people')");
    }
    return withStatus(controller_.setActor(id));
}

// ---- work items -----------------------------------------------------------------------

Reply LineInterpreter::listItems(bool availableOnly) const {
    Reply reply;
    const SessionView& view = controller_.view();
    if (view.instanceId.empty()) {
        return invalid("open or start an instance first");
    }
    for (std::size_t i = 0; i < view.workItems.size(); ++i) {
        const WorkItemView& item = view.workItems[i];
        if (availableOnly && item.trackerStatus != "available") {
            continue;
        }
        std::string line = "  " + pad(number(i + 1) + ".", 4) + pad(item.name, 16) + pad(item.state, 11) +
                            pad(item.assigneeName.empty() ? "-" : item.assigneeName, 8) + "role " + pad(item.requiredRole, 9);
        if (!item.waitingOn.empty()) {
            line += "waiting on " + joinList(item.waitingOn);
        } else if (item.dueAt >= 0) {
            line += "due t=" + numberInt(item.dueAt);
        }
        reply.lines.push_back(line);
    }
    if (availableOnly && reply.lines.empty()) {
        reply.status = StatusMessage::info(view.instance.complete ? "No available work: the instance is complete"
                                                                  : "No available work right now");
    }
    return reply;
}

Reply LineInterpreter::showItem(const std::vector<std::string>& args) const {
    if (controller_.view().instanceId.empty()) {
        return invalid("open or start an instance first");
    }
    std::size_t used = 0;
    std::string id = resolveItem(args, used);
    if (id.empty() || used != args.size()) {
        return invalid("which work item? e.g. show 2 (numbers come from 'items')");
    }
    const std::vector<WorkItemView>& items = controller_.view().workItems;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].id != id) {
            continue;
        }
        const WorkItemView& item = items[i];
        Reply reply;
        reply.lines.push_back(item.name);
        reply.lines.push_back("  state:       " + item.state +
                              (item.trackerStatus.empty() ? "" : "  (" + item.trackerStatus + ")"));
        reply.lines.push_back("  role:        " + item.requiredRole);
        reply.lines.push_back("  assigned to: " + (item.assigneeName.empty() ? std::string("nobody") : item.assigneeName));
        if (!item.waitingOn.empty()) {
            reply.lines.push_back("  waiting on:  " + joinList(item.waitingOn));
        }
        if (item.dueAt >= 0) {
            reply.lines.push_back("  due:         t=" + numberInt(item.dueAt) + " (now t=" +
                                  numberInt(controller_.view().clock) + ")");
        }
        std::vector<WorkAction> hints = InterfaceController::hintedActions(item.state);
        std::vector<std::string> names;
        for (std::size_t h = 0; h < hints.size(); ++h) {
            names.push_back(actionName(hints[h]));
        }
        reply.lines.push_back("  can do:      " + (names.empty() ? std::string("nothing") : joinList(names)));
        return reply;
    }
    return invalid("no such work item");
}

Reply LineInterpreter::workAction(WorkAction action, const std::vector<std::string>& args) {
    if (controller_.view().instanceId.empty()) {
        return invalid("open or start an instance first");
    }
    std::size_t used = 0;
    std::string item = resolveItem(args, used);
    if (item.empty()) {
        return invalid(args.empty() ? "which work item? e.g. complete 2 or complete Check Stock"
                                    : "no work item called '" + joinWords(args, 0, args.size()) + "' (type 'items')");
    }

    std::vector<std::string> rest(args.begin() + static_cast<long>(used), args.end());
    std::string assignee;
    std::string reason;
    if (action == WorkAction::Assign) {
        if (!rest.empty()) {
            if (lower(rest[0]) != "to" || rest.size() < 2) {
                return invalid("usage: assign <item> [to <person>]");
            }
            assignee = resolvePerson(std::vector<std::string>(rest.begin() + 1, rest.end()));
            if (assignee.empty()) {
                return invalid("no participant called '" + joinWords(rest, 1, rest.size()) + "' (type 'people')");
            }
        }
    } else if (action == WorkAction::Reject) {
        reason = joinWords(rest, 0, rest.size());
    } else if (!rest.empty()) {
        return invalid("unexpected '" + joinWords(rest, 0, rest.size()) + "' after the item");
    }

    Reply reply = withStatus(controller_.perform(action, item, assignee, reason));
    if (!reply.status.isError()) {
        reply.lines = outcomeLines();
    }
    return reply;
}

// ---- approvals, time, history, reports -------------------------------------------------

Reply LineInterpreter::listApprovals() const {
    const SessionView& view = controller_.view();
    if (view.instanceId.empty()) {
        return invalid("open or start an instance first");
    }
    Reply reply;
    for (std::size_t i = 0; i < view.approvals.size(); ++i) {
        const ApprovalView& a = view.approvals[i];
        std::string line = "  " + pad(a.id, 8) + pad(a.status, 10) + pad("'" + a.workItemId + "'", 12) + "needs " +
                           pad(a.approverRole, 9) + a.summary;
        if (!a.decidedBy.empty()) {
            line += "  (by " + a.decidedBy + (a.reason.empty() ? "" : ": " + a.reason) + ")";
        }
        reply.lines.push_back(line);
    }
    if (reply.lines.empty()) {
        reply.status = StatusMessage::info("No approval requests in this instance");
    }
    return reply;
}

Reply LineInterpreter::decideApproval(bool approve, const std::vector<std::string>& args) {
    if (args.empty()) {
        return invalid(approve ? "usage: approve <id>, e.g. approve APR-1"
                               : "usage: reject <id> <reason>, e.g. reject APR-1 over budget");
    }
    if (approve && args.size() != 1) {
        return invalid("usage: approve <id>");
    }
    std::string id = args[0];
    for (std::size_t i = 0; i < id.size(); ++i) {
        id[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(id[i])));
    }
    StatusMessage status = approve ? controller_.approve(id)
                                   : controller_.rejectApproval(id, joinWords(args, 1, args.size()));
    Reply reply = withStatus(status);
    if (!status.isError()) {
        reply.lines = outcomeLines();
    }
    return reply;
}

Reply LineInterpreter::advance(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return invalid("usage: advance <units>, e.g. advance 3");
    }
    Reply reply;
    reply.status = controller_.advance(args[0], reply.lines);
    for (std::size_t i = 0; i < reply.lines.size(); ++i) {
        reply.lines[i] = "  " + reply.lines[i];
    }
    return reply;
}

Reply LineInterpreter::listEvents() {
    Reply reply;
    const std::vector<std::string> types = controller_.eventTypes();
    if (types.empty()) {
        reply.status = StatusMessage::info("This industry raises no events (choose an industry first)");
        return reply;
    }
    for (std::size_t i = 0; i < types.size(); ++i) {
        reply.lines.push_back("  " + types[i]);
    }
    reply.status = StatusMessage::info("Raise one with: event <TYPE> [what happened]");
    return reply;
}

Reply LineInterpreter::raiseEvent(const std::vector<std::string>& args) {
    if (args.empty()) {
        return invalid("usage: event <TYPE> [what happened], e.g. event WORM_SIGN");
    }
    Reply reply;
    reply.status = controller_.raiseEvent(args[0], joinWords(args, 1, args.size()), reply.lines);
    for (std::size_t i = 0; i < reply.lines.size(); ++i) {
        reply.lines[i] = "  " + reply.lines[i];
    }
    return reply;
}

Reply LineInterpreter::undo() {
    Reply reply;
    reply.status = controller_.undo();
    return reply;
}

Reply LineInterpreter::history(const std::vector<std::string>& args) {
    bool newest = !args.empty() && lower(args[0]) == "newest";
    std::string filter = joinWords(args, newest ? 1 : 0, args.size());
    Reply reply;
    std::vector<std::string> lines;
    reply.status = controller_.history(lines, newest, filter);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        reply.lines.push_back("  " + pad(number(i + 1), 4) + lines[i]);
    }
    return reply;
}

Reply LineInterpreter::report() {
    std::string progress;
    std::vector<std::string> audit;
    std::vector<std::string> overdue;
    Reply reply;
    reply.status = controller_.reports(progress, audit, overdue);
    if (reply.status.isError()) {
        return reply;
    }
    reply.lines.push_back("Progress: " + progress);
    reply.lines.push_back("Audit:");
    for (std::size_t i = 0; i < audit.size(); ++i) {
        reply.lines.push_back("  " + audit[i]);
    }
    reply.lines.push_back("Overdue (escalated): " + (overdue.empty() ? std::string("none") : joinList(overdue)));
    return reply;
}

// ---- helpers --------------------------------------------------------------------------

std::string LineInterpreter::resolveItem(const std::vector<std::string>& args, std::size_t& used) const {
    used = 0;
    const std::vector<WorkItemView>& items = controller_.view().workItems;
    if (args.empty()) {
        return "";
    }
    if (allDigits(args[0])) {
        std::size_t index = 0;
        if (!parseMenuChoice(args[0], items.size(), index)) {
            return "";
        }
        used = 1;
        return items[index].id;
    }
    // Longest run of leading words that names an item, so "Check Stock out of date" works.
    for (std::size_t count = args.size(); count >= 1; --count) {
        std::string name = lower(joinWords(args, 0, count));
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (lower(items[i].name) == name || lower(items[i].id) == name) {
                used = count;
                return items[i].id;
            }
        }
    }
    return "";
}

std::string LineInterpreter::resolvePerson(const std::vector<std::string>& words) const {
    const std::vector<ParticipantView>& people = controller_.view().participants;
    std::string wanted = lower(joinWords(words, 0, words.size()));
    if (allDigits(wanted)) {
        std::size_t index = 0;
        return parseMenuChoice(wanted, people.size(), index) ? people[index].id : "";
    }
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (lower(people[i].id) == wanted || lower(people[i].name) == wanted) {
            return people[i].id;
        }
    }
    return "";
}

std::vector<std::string> LineInterpreter::outcomeLines() const {
    std::vector<std::string> lines;
    if (!controller_.hasOutcome()) {
        return lines;
    }
    const ActionOutcome& o = controller_.lastOutcome();
    if (!o.previousState.empty() && o.previousState != o.newState) {
        lines.push_back("  state:     '" + o.workItemId + "' " + o.previousState + " -> " + o.newState);
    }
    if (!o.routingDescription.empty()) {
        lines.push_back("  routing:   " + o.routingDescription);
    }
    if (!o.madeAvailable.empty()) {
        lines.push_back("  available: " + joinList(o.madeAvailable));
    }
    const std::vector<WorkItemView>& items = controller_.view().workItems;
    for (std::size_t w = 0; w < o.nowWaiting.size(); ++w) {
        std::string needs;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (items[i].id == o.nowWaiting[w]) {
                needs = joinList(items[i].waitingOn);
            }
        }
        lines.push_back("  waiting:   " + o.nowWaiting[w] + (needs.empty() ? "" : " (needs " + needs + ")"));
    }
    if (!o.reopened.empty()) {
        lines.push_back("  reopened:  " + joinList(o.reopened));
    }
    if (o.instanceComplete) {
        lines.push_back("  the instance is complete");
    }
    return lines;
}

} // namespace ui
} // namespace tividy
