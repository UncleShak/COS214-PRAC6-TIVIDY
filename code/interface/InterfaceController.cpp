#include "InterfaceController.h"

#include <cstdio>
#include <stdexcept>

#include "HistoryIterator.h"
#include "InputValidation.h"

namespace tividy {
namespace ui {

namespace {

/**
 * Turns the exception being handled into a status line. Team convention: catch
 * std::exception once and show what() unchanged. Must be called from inside a catch block.
 */
StatusMessage describeCurrentException() {
    try {
        throw;
    } catch (const std::exception& e) {
        return StatusMessage::engineError(e.what());
    } catch (...) {
        return StatusMessage::engineError("unknown error");
    }
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] == value) {
            return true;
        }
    }
    return false;
}

} // namespace

InterfaceController::InterfaceController(EngineGateway& engine)
    : engine_(engine), hasOutcome_(false) {}

StatusMessage InterfaceController::refresh() {
    try {
        view_.industries = engine_.listIndustries();

        view_.instances.clear();
        view_.participants.clear();
        if (!view_.industryId.empty()) {
            view_.instances = engine_.listInstances(view_.industryId);
            view_.participants = engine_.listParticipants(view_.industryId);
        }
        if (participantName(view_.actorId).empty()) {
            view_.actorId = view_.participants.empty() ? "" : view_.participants.front().id;
        }

        view_.workItems.clear();
        view_.availableWork.clear();
        view_.approvals.clear();
        if (!view_.instanceId.empty()) {
            view_.instance = engine_.getInstance(view_.instanceId);
            view_.workItems = engine_.listWorkItems(view_.instanceId);
            view_.availableWork = engine_.listAvailableWork(view_.instanceId);
            view_.approvals = engine_.listApprovals(view_.instanceId);
        }
        view_.clock = engine_.currentTime();
    } catch (...) {
        return status_ = describeCurrentException();
    }
    return StatusMessage::none();
}

StatusMessage InterfaceController::selectIndustry(const std::string& industryId) {
    bool known = false;
    for (std::size_t i = 0; i < view_.industries.size(); ++i) {
        if (view_.industries[i].id == industryId) {
            known = true;
        }
    }
    if (!known) {
        return status_ = StatusMessage::invalidInput("unknown industry '" + industryId + "'");
    }
    view_.industryId = industryId;
    view_.instanceId.clear();
    view_.actorId.clear();
    hasOutcome_ = false;

    StatusMessage refreshed = refresh();
    if (refreshed.isError()) {
        return refreshed;
    }
    return status_ = StatusMessage::none();
}

StatusMessage InterfaceController::startFields(const std::string& definition,
                                               std::vector<DataField>& fields) {
    StatusMessage problem = requireIndustry();
    if (problem.isError()) {
        return status_ = problem;
    }
    try {
        fields = engine_.startFields(view_.industryId, definition);
    } catch (...) {
        return status_ = describeCurrentException();
    }
    return StatusMessage::none();
}

StatusMessage InterfaceController::startInstance(const std::string& definition,
                                                 const std::map<std::string, std::string>& values) {
    StatusMessage problem = requireIndustry();
    if (problem.isError()) {
        return status_ = problem;
    }

    const IndustryInfo* industry = 0;
    for (std::size_t i = 0; i < view_.industries.size(); ++i) {
        if (view_.industries[i].id == view_.industryId) {
            industry = &view_.industries[i];
        }
    }
    if (industry == 0 || !contains(industry->definitions, definition)) {
        return status_ = StatusMessage::invalidInput("unknown workflow definition '" + definition + "'");
    }

    std::vector<DataField> fields;
    problem = startFields(definition, fields);
    if (problem.isError()) {
        return problem;
    }

    std::map<std::string, std::string> cleanedValues;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        std::map<std::string, std::string>::const_iterator typed = values.find(fields[i].key);
        std::string cleaned;
        std::string error = validateField(fields[i], typed == values.end() ? "" : typed->second, cleaned);
        if (!error.empty()) {
            return status_ = StatusMessage::invalidInput(error);
        }
        cleanedValues[fields[i].key] = cleaned;
    }

    std::string instanceId;
    try {
        instanceId = engine_.startInstance(view_.industryId, definition, cleanedValues);
    } catch (...) {
        return refuse();
    }

    hasOutcome_ = false;
    view_.instanceId = instanceId;
    StatusMessage refreshed = refresh();
    if (refreshed.isError()) {
        return refreshed;
    }
    return status_ = StatusMessage::success("Started " + definition + " as " + instanceId);
}

StatusMessage InterfaceController::selectInstance(const std::string& instanceId) {
    StatusMessage problem = requireIndustry();
    if (problem.isError()) {
        return status_ = problem;
    }
    if (trim(instanceId).empty()) {
        return status_ = StatusMessage::invalidInput("choose an instance first");
    }

    std::string previous = view_.instanceId;
    view_.instanceId = instanceId;
    StatusMessage refreshed = refresh();
    if (refreshed.isError()) {
        view_.instanceId = previous;
        refresh();
        return status_ = refreshed;
    }
    if (previous != instanceId) {
        hasOutcome_ = false;
    }
    return status_ = StatusMessage::none();
}

StatusMessage InterfaceController::setActor(const std::string& participantId) {
    if (participantName(participantId).empty()) {
        return status_ = StatusMessage::invalidInput("unknown participant '" + participantId + "'");
    }
    view_.actorId = participantId;
    return status_ = StatusMessage::info("Acting as " + participantName(participantId));
}

StatusMessage InterfaceController::perform(WorkAction action, const std::string& workItemId,
                                           const std::string& assigneeId, const std::string& reason) {
    StatusMessage problem = requireInstance();
    if (problem.isError()) {
        return status_ = problem;
    }
    if (trim(workItemId).empty()) {
        return status_ = StatusMessage::invalidInput("choose a work item first");
    }
    if (view_.actorId.empty()) {
        return status_ = StatusMessage::invalidInput("no participant is selected to act");
    }

    ActionRequest request;
    request.instanceId = view_.instanceId;
    request.workItemId = workItemId;
    request.action = action;
    request.actorId = view_.actorId;

    if (action == WorkAction::Assign && !assigneeId.empty()) {
        if (participantName(assigneeId).empty()) {
            return status_ = StatusMessage::invalidInput("unknown participant '" + assigneeId + "'");
        }
        request.assigneeId = assigneeId;
    }
    if (action == WorkAction::Reject) {
        request.reason = trim(reason);
        if (request.reason.empty()) {
            return status_ = StatusMessage::invalidInput("a reason is required to reject work");
        }
        if (request.reason.size() > kMaxTextLength) {
            return status_ = StatusMessage::invalidInput("the reason is too long (at most 200 characters)");
        }
    }

    try {
        return accept(engine_.act(request));
    } catch (...) {
        return refuse();
    }
}

StatusMessage InterfaceController::approve(const std::string& approvalId) {
    StatusMessage problem = requireInstance();
    if (problem.isError()) {
        return status_ = problem;
    }
    if (trim(approvalId).empty()) {
        return status_ = StatusMessage::invalidInput("give the approval ID, for example: approve APR-1");
    }
    if (view_.actorId.empty()) {
        return status_ = StatusMessage::invalidInput("no participant is selected to act");
    }
    try {
        return accept(engine_.approve(view_.instanceId, trim(approvalId), view_.actorId));
    } catch (...) {
        return refuse();
    }
}

StatusMessage InterfaceController::rejectApproval(const std::string& approvalId, const std::string& reason) {
    StatusMessage problem = requireInstance();
    if (problem.isError()) {
        return status_ = problem;
    }
    if (trim(approvalId).empty()) {
        return status_ = StatusMessage::invalidInput("give the approval ID, for example: reject APR-1 too expensive");
    }
    std::string why = trim(reason);
    if (why.empty()) {
        return status_ = StatusMessage::invalidInput("a reason is required to reject an approval");
    }
    if (why.size() > kMaxTextLength) {
        return status_ = StatusMessage::invalidInput("the reason is too long (at most 200 characters)");
    }
    if (view_.actorId.empty()) {
        return status_ = StatusMessage::invalidInput("no participant is selected to act");
    }
    try {
        return accept(engine_.rejectApproval(view_.instanceId, trim(approvalId), view_.actorId, why));
    } catch (...) {
        return refuse();
    }
}

StatusMessage InterfaceController::advance(const std::string& unitsText, std::vector<std::string>& changes) {
    changes.clear();
    std::size_t index = 0;
    if (!parseMenuChoice(unitsText, 1000, index)) {
        return status_ = StatusMessage::invalidInput("advance needs a whole number of time units from 1 to 1000");
    }
    const int units = static_cast<int>(index) + 1;
    try {
        changes = engine_.advanceClock(units);
    } catch (...) {
        return refuse();
    }
    StatusMessage refreshed = refresh();
    if (refreshed.isError()) {
        return refreshed;
    }
    char text[96];
    std::snprintf(text, sizeof(text), "Clock advanced by %d to t=%d%s", units, view_.clock,
                  changes.empty() ? ", nothing became overdue" : "");
    return status_ = StatusMessage::success(text);
}

StatusMessage InterfaceController::accept(const ActionOutcome& outcome) {
    outcome_ = outcome;
    hasOutcome_ = true;
    StatusMessage refreshed = refresh();
    if (refreshed.isError()) {
        return refreshed;
    }

    std::string text = outcome.commandDescription;
    if (!outcome.previousState.empty() && outcome.previousState != outcome.newState) {
        text += " (" + outcome.previousState + " -> " + outcome.newState + ")";
    }
    if (outcome.instanceComplete) {
        text += ". Instance complete.";
    }
    return status_ = StatusMessage::success(text);
}

StatusMessage InterfaceController::refuse() {
    hasOutcome_ = false;
    StatusMessage refused = describeCurrentException();
    refresh();
    return status_ = refused;
}

StatusMessage InterfaceController::history(std::vector<std::string>& lines, bool newestFirst,
                                           const std::string& filter) {
    lines.clear();
    StatusMessage problem = requireInstance();
    if (problem.isError()) {
        return status_ = problem;
    }

    std::vector<std::string> entries;
    try {
        entries = engine_.history(view_.instanceId);
    } catch (...) {
        return status_ = describeCurrentException();
    }

    HistoryIterator it(entries, newestFirst ? HistoryIterator::NewestFirst : HistoryIterator::OldestFirst,
                       trim(filter));
    for (it.first(); !it.isDone(); it.next()) {
        lines.push_back(it.currentItem());
    }

    if (lines.empty()) {
        return StatusMessage::info(entries.empty() ? "No history yet"
                                                   : "No history entries contain '" + trim(filter) + "'");
    }
    return StatusMessage::none();
}

StatusMessage InterfaceController::reports(std::string& progress, std::vector<std::string>& audit,
                                           std::vector<std::string>& overdue) {
    StatusMessage problem = requireInstance();
    if (problem.isError()) {
        return status_ = problem;
    }
    try {
        progress = engine_.progressSummary(view_.instanceId);
        audit = engine_.auditReport(view_.instanceId);
        overdue = engine_.overdueWork(view_.instanceId);
    } catch (...) {
        return status_ = describeCurrentException();
    }
    return StatusMessage::none();
}

std::vector<WorkAction> InterfaceController::hintedActions(const std::string& state) {
    std::vector<WorkAction> actions;
    if (state == "Available") {
        actions.push_back(WorkAction::Assign);
        actions.push_back(WorkAction::Cancel);
        actions.push_back(WorkAction::Escalate);
    } else if (state == "Assigned") {
        actions.push_back(WorkAction::Start);
        actions.push_back(WorkAction::Cancel);
    } else if (state == "Started") {
        actions.push_back(WorkAction::Complete);
        actions.push_back(WorkAction::Reject);
        actions.push_back(WorkAction::Cancel);
    } else if (state == "Escalated") {
        actions.push_back(WorkAction::Assign);
        actions.push_back(WorkAction::Cancel);
    }
    return actions;
}

std::string InterfaceController::participantName(const std::string& participantId) const {
    for (std::size_t i = 0; i < view_.participants.size(); ++i) {
        if (view_.participants[i].id == participantId) {
            return view_.participants[i].name;
        }
    }
    return "";
}

StatusMessage InterfaceController::requireIndustry() const {
    if (view_.industryId.empty()) {
        return StatusMessage::invalidInput("choose an industry first");
    }
    return StatusMessage::none();
}

StatusMessage InterfaceController::requireInstance() const {
    StatusMessage problem = requireIndustry();
    if (problem.isError()) {
        return problem;
    }
    if (view_.instanceId.empty()) {
        return StatusMessage::invalidInput("choose or start an instance first");
    }
    return StatusMessage::none();
}

} // namespace ui
} // namespace tividy
