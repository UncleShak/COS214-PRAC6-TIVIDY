#include "RealEngine.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "ApprovalHandler.h"
#include "ApprovalRequest.h"
#include "AuditReportVisitor.h"
#include "IndustryCatalog.h"
#include "IndustryFactory.h"
#include "KeyValuePayload.h"
#include "OverdueWorkVisitor.h"
#include "Participant.h"
#include "ProgressReportVisitor.h"
#include "Stage.h"
#include "WorkItem.h"
#include "WorkflowVisitor.h"
#include "WorkItemState.h"
#include "WorkflowDefinition.h"
#include "WorkflowEngine.h"
#include "WorkflowInstance.h"

namespace tividy {
namespace ui {

namespace {

// "estimatedYieldTonnes" -> "Estimated yield tonnes"
std::string labelFor(const std::string& key) {
    std::string label;
    for (std::size_t i = 0; i < key.size(); ++i) {
        const char c = key[i];
        if (i > 0 && std::isupper(static_cast<unsigned char>(c))) {
            label += ' ';
            label += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            label += (i == 0) ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        }
    }
    return label;
}

bool isNumber(const std::string& text) {
    if (text.empty()) return false;
    char* end = 0;
    std::strtod(text.c_str(), &end);
    return end != 0 && *end == '\0';
}

// The example data writes yes/no answers as "yes" or "no".
bool isYesNo(const std::string& text) { return text == "yes" || text == "no"; }

std::string approvalKey(int id) { return "APR-" + std::to_string(id); }

// Accepts "APR-3", "apr-3" or "3".
int parseApprovalId(const std::string& text) {
    std::string digits = text;
    if (digits.size() > 4 && (digits.compare(0, 4, "APR-") == 0 || digits.compare(0, 4, "apr-") == 0)) digits = digits.substr(4);
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos || digits.size() > 6)
        throw std::invalid_argument("No approval called '" + text + "' (use an id like APR-1)");
    return std::atoi(digits.c_str());
}

std::string stateOf(const WorkflowInstance& inst, const std::string& item) {
    return inst.getItem(item)->getCurrentState()->getStateName();
}

}  // namespace

RealEngine::RealEngine() : engine(new WorkflowEngine()), factories(createAllIndustries()) {
    for (std::size_t i = 0; i < factories.size(); ++i) {
        const std::size_t before = engine->getParticipants().size();
        const std::string name = engine->installIndustry(*factories[i]);
        const std::vector<Participant*> everyone = engine->getParticipants();
        for (std::size_t p = before; p < everyone.size(); ++p) participantsByIndustry[name].push_back(everyone[p]->getID());
    }
}

RealEngine::~RealEngine() {}

const IndustryFactory& RealEngine::industry(const std::string& industryId) const {
    for (std::size_t i = 0; i < factories.size(); ++i)
        if (factories[i]->getWorkflowName() == industryId) return *factories[i];
    throw std::invalid_argument("No industry called '" + industryId + "'");
}

WorkflowInstance& RealEngine::instance(const std::string& instanceId) const {
    WorkflowInstance* found = engine->getInstance(instanceId);
    if (found == 0) throw std::invalid_argument("No instance called '" + instanceId + "'");
    return *found;
}

// ---- industries and instances ----

std::vector<IndustryInfo> RealEngine::listIndustries() const {
    std::vector<IndustryInfo> all;
    for (std::size_t i = 0; i < factories.size(); ++i) {
        IndustryInfo info;
        info.id = factories[i]->getWorkflowName();
        info.displayName = factories[i]->getDisplayName();
        info.description = factories[i]->getDescription();
        info.sector = factories[i]->getSector();
        info.definitions.push_back(factories[i]->getWorkflowName());
        all.push_back(info);
    }
    return all;
}

namespace {

// Collects work-item names in definition order (Visitor over the definition's root stage).
class ItemOrder : public WorkflowVisitor {
public:
    std::vector<std::string> names;
    void visitWorkItem(WorkItem* item) override { names.push_back(item->getName()); }
    void visitStage(Stage*) override {}
};

} // namespace

std::vector<RouteStop> RealEngine::routeMap(const std::string& industryId, const std::string& definitionName) const {
    const IndustryFactory& factory = industry(industryId);
    if (definitionName != factory.getWorkflowName())
        throw std::invalid_argument("No workflow called '" + definitionName + "' in " + factory.getDisplayName());
    const WorkflowDefinition* definition = engine->getDefinition(factory.getWorkflowName());
    ItemOrder order;
    definition->getRootStage()->accept(&order);

    std::map<std::string, std::string> splitsFrom;   // exception item -> main stop it leaves from
    const std::vector<WorkflowDefinition::ExceptionRoute>& routes = definition->getExceptionRoutes();
    for (std::size_t r = 0; r < routes.size(); ++r)
        for (std::size_t i = 0; i < routes[r].items.size(); ++i)
            splitsFrom.insert(std::make_pair(routes[r].items[i], routes[r].splitsFrom));

    std::vector<RouteStop> stops;
    for (std::size_t i = 0; i < order.names.size(); ++i) {
        RouteStop stop;
        stop.workItemId = order.names[i];
        std::map<std::string, std::string>::const_iterator it = splitsFrom.find(order.names[i]);
        stop.exception = it != splitsFrom.end();
        if (stop.exception) stop.splitsFrom = it->second;
        stops.push_back(stop);
    }
    return stops;
}

std::vector<DataField> RealEngine::startFields(const std::string& industryId, const std::string& definitionName) const {
    const IndustryFactory& factory = industry(industryId);
    if (definitionName != factory.getWorkflowName())
        throw std::invalid_argument("No workflow called '" + definitionName + "' in " + factory.getDisplayName());
    // The industry's example data is the form: each field, with its example as the default.
    const std::string placeholder = factory.getIdPrefix() + "-?";
    std::unique_ptr<DataPayload> example = factory.createInitialData(placeholder);
    const KeyValuePayload* fields = dynamic_cast<const KeyValuePayload*>(example.get());
    std::vector<DataField> form;
    if (fields == 0) return form;
    for (std::map<std::string, std::string>::const_iterator it = fields->entries().begin(); it != fields->entries().end(); ++it) {
        if (it->second == placeholder) continue;   // the case id is generated, not typed
        DataField field;
        field.key = it->first;
        field.label = labelFor(it->first);
        field.numeric = isNumber(it->second);
        field.yesNo = isYesNo(it->second);
        field.required = false;
        field.defaultValue = it->second;
        form.push_back(field);
    }
    return form;
}

std::string RealEngine::startInstance(const std::string& industryId, const std::string& definitionName,
                                      const std::map<std::string, std::string>& initialData) {
    const IndustryFactory& factory = industry(industryId);
    if (definitionName != factory.getWorkflowName())
        throw std::invalid_argument("No workflow called '" + definitionName + "' in " + factory.getDisplayName());
    std::string id;
    do {
        id = factory.getIdPrefix() + "-" + std::to_string(++nextNumber[industryId]);
    } while (engine->getInstance(id) != 0);

    const WorkflowDefinition* definition = engine->getDefinition(factory.getWorkflowName());
    if (!definition->getIdentityField().empty() && initialData.count(definition->getIdentityField()))
        throw std::invalid_argument("The workflow identity is generated by the system");
    std::unique_ptr<DataPayload> data = factory.createInitialData(id);
    KeyValuePayload* fields = dynamic_cast<KeyValuePayload*>(data.get());
    for (std::map<std::string, std::string>::const_iterator it = initialData.begin(); it != initialData.end(); ++it) {
        if (fields == 0) throw std::invalid_argument("This workflow takes no starting data");
        if (!it->second.empty()) fields->set(it->first, it->second);
    }
    engine->startInstance(id, factory.getWorkflowName(), data.get());
    industryOfInstance[id] = industryId;
    return id;
}

InstanceView RealEngine::instanceView(const WorkflowInstance& inst) const {
    InstanceView view;
    view.id = inst.getId();
    view.industryId = industryOfInstance.count(inst.getId()) ? industryOfInstance.find(inst.getId())->second : "";
    view.definitionName = inst.getDefinition()->getRootStage()->getName();
    view.definitionVersion = inst.getDefinition()->getVersion();
    view.complete = inst.isComplete();
    view.stalled = inst.isStalled();
    view.data = inst.getDataFields();
    return view;
}

std::vector<InstanceView> RealEngine::listInstances(const std::string& industryId) const {
    industry(industryId);
    std::vector<InstanceView> views;
    const std::vector<std::string> ids = engine->listInstances();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        std::map<std::string, std::string>::const_iterator owner = industryOfInstance.find(ids[i]);
        if (owner != industryOfInstance.end() && owner->second == industryId) views.push_back(instanceView(instance(ids[i])));
    }
    return views;
}

InstanceView RealEngine::getInstance(const std::string& instanceId) const { return instanceView(instance(instanceId)); }

// ---- work items ----

WorkItemView RealEngine::itemView(const WorkflowInstance& inst, const std::string& name) const {
    const WorkItem* item = inst.getItem(name);
    const ProgressionTracker& progress = inst.getProgress();
    WorkItemView view;
    view.id = name;
    view.name = name;
    view.state = item->getCurrentState()->getStateName();
    if (progress.isAvailable(name)) view.trackerStatus = "available";
    else if (progress.isWaiting(name)) view.trackerStatus = "waiting";
    else if (progress.isCompleted(name)) view.trackerStatus = "completed";
    else if (progress.isRejected(name)) view.trackerStatus = "rejected";
    else if (progress.isCancelled(name)) view.trackerStatus = "cancelled";
    view.requiredRole = item->getRequiredRole();
    view.assigneeName = item->getAssignedParticipant() != 0 ? item->getAssignedParticipant()->getName() : "";
    if (view.trackerStatus == "waiting") view.waitingOn = progress.unmetPrerequisites(name);
    view.dueAt = inst.getDueTime(name);
    return view;
}

std::vector<WorkItemView> RealEngine::listWorkItems(const std::string& instanceId) const {
    const WorkflowInstance& inst = instance(instanceId);
    std::vector<WorkItemView> views;
    const std::vector<WorkItem*> items = inst.getItems();
    for (std::size_t i = 0; i < items.size(); ++i) views.push_back(itemView(inst, items[i]->getName()));
    return views;
}

std::vector<WorkItemView> RealEngine::listAvailableWork(const std::string& instanceId) const {
    const WorkflowInstance& inst = instance(instanceId);
    std::vector<WorkItemView> views;
    const std::vector<std::string> names = inst.getAvailableWork();
    for (std::size_t i = 0; i < names.size(); ++i) views.push_back(itemView(inst, names[i]));
    return views;
}

WorkItemView RealEngine::getWorkItem(const std::string& instanceId, const std::string& workItemId) const {
    return itemView(instance(instanceId), workItemId);
}

std::vector<ParticipantView> RealEngine::listParticipants(const std::string& industryId) const {
    industry(industryId);
    std::vector<ParticipantView> views;
    std::map<std::string, std::vector<std::string> >::const_iterator ids = participantsByIndustry.find(industryId);
    if (ids == participantsByIndustry.end()) return views;
    for (std::size_t i = 0; i < ids->second.size(); ++i) {
        const Participant* person = engine->getParticipant(ids->second[i]);
        ParticipantView view;
        view.id = person->getID();
        view.name = person->getName();
        view.roles = person->getRoleNames();
        view.workload = person->getWorkload();
        views.push_back(view);
    }
    return views;
}

// ---- actions ----

ActionOutcome RealEngine::outcomeFor(const WorkflowInstance& inst, const std::string& workItem,
                                     const std::string& previousState, std::size_t commandsBefore) const {
    ActionOutcome outcome;
    outcome.workItemId = workItem;
    outcome.previousState = previousState;
    outcome.newState = stateOf(inst, workItem);
    const std::vector<std::string> commands = inst.getCommandLog();
    if (commands.size() > commandsBefore) outcome.commandDescription = commands.back();
    if (inst.routedLastAction()) {
        const ProgressionResult& routed = inst.getLastProgression();
        outcome.routingDescription = routed.decision.description;
        outcome.madeAvailable = routed.madeAvailable;
        outcome.nowWaiting = routed.waiting;
        outcome.reopened = routed.reopened;
    }
    outcome.instanceComplete = inst.isComplete();
    return outcome;
}

ActionOutcome RealEngine::act(const ActionRequest& request) {
     WorkflowInstance& inst = instance(request.instanceId);
    const std::string before = stateOf(inst, request.workItemId);
    const std::size_t commandsBefore = inst.getCommandLog().size();
    switch (request.action) {
    case WorkAction::Assign:
        if (request.assigneeId.empty()) engine->autoAssign(request.instanceId, request.workItemId, request.actorId);
        else engine->assignWork(request.instanceId, request.workItemId, request.actorId, request.assigneeId);
        break;
    case WorkAction::Start:
        engine->startWork(request.instanceId, request.workItemId, request.actorId);
        break;
    case WorkAction::Complete:
        engine->completeWorkItem(request.instanceId, request.workItemId, request.actorId);
        break;
    case WorkAction::Reject:
        if (request.reason.empty()) throw std::invalid_argument("A rejection needs a reason");
        engine->rejectWork(request.instanceId, request.workItemId, request.actorId, request.reason);
        break;
    case WorkAction::Cancel:
        engine->cancelWork(request.instanceId, request.workItemId, request.actorId);
        break;
    case WorkAction::Escalate:
        engine->escalateWork(request.instanceId, request.workItemId, request.actorId);
        break;
    }
    return outcomeFor(inst, request.workItemId, before, commandsBefore);
}

std::vector<std::string> RealEngine::history(const std::string& instanceId) const {
    return instance(instanceId).getHistory().toStringList();
}

// ---- reports (Visitor) ----

std::string RealEngine::progressSummary(const std::string& instanceId) const {
    ProgressReportVisitor report;
    instance(instanceId).accept(&report);
    return report.summary();
}

std::vector<std::string> RealEngine::auditReport(const std::string& instanceId) const {
    AuditReportVisitor report;
    instance(instanceId).accept(&report);
    return report.lines();
}

std::vector<std::string> RealEngine::overdueWork(const std::string& instanceId) const {
    OverdueWorkVisitor report;
    instance(instanceId).accept(&report);
    return report.overdueNames();
}

// ---- approvals (Chain of Responsibility) ----

std::vector<ApprovalView> RealEngine::listApprovals(const std::string& instanceId) const {
    std::vector<ApprovalView> views;
    const std::vector<const ApprovalRequest*> requests = instance(instanceId).getApprovals();
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const ApprovalRequest* request = requests[i];
        ApprovalView view;
        view.id = approvalKey(request->getId());
        view.workItemId = request->getItemName();
        std::ostringstream summary;
        summary << request->getReason() << " (value " << request->getValue() << ", requested by "
                << request->getRequesterName() << ")";
        view.summary = summary.str();
        view.approverRole = request->getHandler() != 0 ? request->getHandler()->getRole() : "";
        view.status = statusName(request->getStatus());
        view.decidedBy = request->getDecidedBy();
        if (request->getStatus() != APPROVED) view.reason = request->getDecisionNote();
        views.push_back(view);
    }
    return views;
}

ActionOutcome RealEngine::approve(const std::string& instanceId, const std::string& approvalId,
                                  const std::string& actorId) {
    WorkflowInstance& inst = instance(instanceId);
    const int id = parseApprovalId(approvalId);
    const std::vector<const ApprovalRequest*> requests = inst.getApprovals();
    if (id < 1 || id > static_cast<int>(requests.size())) throw std::invalid_argument("No approval " + approvalKey(id) + " in " + instanceId);
    const std::string item = requests[id - 1]->getItemName();
    const std::string before = stateOf(inst, item);
    const std::size_t commandsBefore = inst.getCommandLog().size();
    engine->approve(instanceId, id, actorId);
    return outcomeFor(inst, item, before, commandsBefore);
}

ActionOutcome RealEngine::rejectApproval(const std::string& instanceId, const std::string& approvalId,
                                         const std::string& actorId, const std::string& reason) {
    WorkflowInstance& inst = instance(instanceId);
    const int id = parseApprovalId(approvalId);
    const std::vector<const ApprovalRequest*> requests = inst.getApprovals();
    if (id < 1 || id > static_cast<int>(requests.size())) throw std::invalid_argument("No approval " + approvalKey(id) + " in " + instanceId);
    const std::string item = requests[id - 1]->getItemName();
    const std::string before = stateOf(inst, item);
    const std::size_t commandsBefore = inst.getCommandLog().size();
    engine->rejectApproval(instanceId, id, actorId, reason);
    return outcomeFor(inst, item, before, commandsBefore);
}

// ---- time ----

std::vector<std::string> RealEngine::advanceClock(int units) { return engine->advanceClock(units); }
int RealEngine::currentTime() const { return engine->currentTime(); }

// ---- events (Observer) and undo (Memento) ----

std::vector<std::string> RealEngine::eventTypes(const std::string& industryId) const {
    return industry(industryId).getEventTypes();
}

std::map<std::string, std::string> RealEngine::states(const WorkflowInstance& inst) const {
    std::map<std::string, std::string> all;
    const std::vector<WorkItem*> items = inst.getItems();
    for (std::size_t i = 0; i < items.size(); ++i) all[items[i]->getName()] = items[i]->getCurrentState()->getStateName();
    return all;
}

std::vector<std::string> RealEngine::raiseEvent(const std::string& instanceId, const std::string& type,
                                                const std::string& description) {
    WorkflowInstance& inst = instance(instanceId);
    const std::map<std::string, std::string> before = states(inst);
    engine->raiseEvent(instanceId, type, description);
    const std::map<std::string, std::string> after = states(inst);
    std::vector<std::string> changes;
    const std::vector<WorkItem*> items = inst.getItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        const std::string& name = items[i]->getName();
        if (before.find(name)->second != after.find(name)->second)
            changes.push_back(name + ": " + before.find(name)->second + " -> " + after.find(name)->second);
    }
    return changes;
}

std::string RealEngine::nextUndo(const std::string& instanceId) const {
    return engine->canUndo(instanceId) ? engine->nextUndoLabel(instanceId) : "";
}

std::string RealEngine::undo(const std::string& instanceId) { return engine->undo(instanceId); }

}  // namespace ui
}  // namespace tividy
