// WorkflowInstance.cpp
#include "WorkflowInstance.h"

#include <stdexcept>

#include "AccessControlProxy.h"
#include "AuditDecorator.h"
#include "DeadlineDecorator.h"
#include "WorkflowException.h"
#include "Commands.h"
#include "Event.h"
#include "InstanceSnapshot.h"
#include "InterruptListener.h"
#include "KeyValuePayload.h"
#include "RoutingCondition.h"
#include "RoutingCondition.h"
#include "Stage.h"
#include "StateFactory.h"
#include "WorkItem.h"
#include "WorkItemState.h"
#include "WorkflowVisitor.h"

namespace {
// Visitor that collects the work items of a tree in order.
class ItemCollector : public WorkflowVisitor {
public:
    std::vector<WorkItem*> found;
    void visitWorkItem(WorkItem* item) override { found.push_back(item); }
    void visitStage(Stage*) override {}
};

bool isOpen(const std::string& state) {
    return state == "Available" || state == "Assigned" || state == "Started" || state == "Escalated";
}

std::string approvalId(int id) { return "APR-" + std::to_string(id); }
}  // namespace

WorkflowInstance::WorkflowInstance(const std::string& id, const WorkflowDefinition* def, const DataPayload* initialData,
                                   const Clock* engineClock)
    : instanceId(id), definition(def), root(nullptr), history(id), system("System", "SYSTEM"), started(false),
      clock(engineClock), routedLast(false) {
    if (id.empty()) throw std::invalid_argument("A workflow instance needs an id");
    if (def == nullptr || def->getRootStage() == nullptr) throw std::invalid_argument("A workflow instance needs a definition");
    if (def->getRoutingRule() == nullptr) throw std::invalid_argument("Definition '" + def->getRootStage()->getName() + "' has no routing rule");

    // Prototype: this instance's own tree, so instances never share work items.
    root = def->getRootStage()->clone();
    try {
        ItemCollector collector;
        root->accept(&collector);
        for (std::size_t i = 0; i < collector.found.size(); ++i) {
            WorkItem* item = collector.found[i];
            items[item->getName()] = item;
            itemOrder.push_back(item->getName());
            item->attach(this);
        }

        // Instance isolation: clone the payload so changes don't affect other instances
        payload.reset(initialData != nullptr ? initialData->clone() : new KeyValuePayload());
        tracker.reset(new ProgressionTracker(*def->getRoutingRule(), def->getDependencies(), def->getEntryItems()));

        if (clock == nullptr) {
            ownClock.reset(new ManualClock(0));
            clock = ownClock.get();
        }
        if (def->getApprovalChain() != nullptr) approvalDesk.reset(new ApprovalDesk(def->getApprovalChain(), clock, &history));

        attach(&history);
        const std::vector<WorkflowDefinition::InterruptRule>& rules = def->getInterruptRules();
        for (std::size_t i = 0; i < rules.size(); ++i) {
            interruptListeners.push_back(std::unique_ptr<InterruptListener>(new InterruptListener(*this, rules[i])));
            attach(interruptListeners.back().get());
        }
    } catch (...) {
        deleteComponentTree(root);
        throw;
    }
}

WorkflowInstance::~WorkflowInstance() {
    deleteComponentTree(root);
}

std::string WorkflowInstance::getId() const { return instanceId; }
const WorkflowDefinition* WorkflowInstance::getDefinition() const { return definition; }
const DataPayload* WorkflowInstance::getPayload() const { return payload.get(); }

std::map<std::string, std::string> WorkflowInstance::getDataFields() const {
    const KeyValuePayload* fields = dynamic_cast<const KeyValuePayload*>(payload.get());
    return fields != nullptr ? fields->entries() : std::map<std::string, std::string>();
}

void WorkflowInstance::setData(const std::string& key, const std::string& value) {
    KeyValuePayload* fields = dynamic_cast<KeyValuePayload*>(payload.get());
    if (fields == nullptr) throw std::logic_error("This instance's data cannot be edited field by field");
    if (key.empty()) throw std::invalid_argument("A data field needs a name");
    fields->set(key, value);
    history.record("Data: " + key + " = " + value);
}

void WorkflowInstance::start() {
    if (started) throw std::logic_error("Instance " + instanceId + " has already started");
    const std::vector<std::string> ready = tracker->start();
    started = true;
    for (std::size_t i = 0; i < ready.size(); ++i) open(ready[i]);
    history.record("Instance " + instanceId + " started");
}

bool WorkflowInstance::isStarted() const { return started; }
bool WorkflowInstance::isComplete() const { return started && tracker->isComplete(); }
bool WorkflowInstance::isStalled() const { return started && tracker->isStalled(); }

void WorkflowInstance::requireStarted() const {
    if (!started) throw std::logic_error("Instance " + instanceId + " has not started");
}

bool WorkflowInstance::hasItem(const std::string& name) const { return items.count(name) > 0; }

WorkItem* WorkflowInstance::getItem(const std::string& name) const {
    std::map<std::string, WorkItem*>::const_iterator found = items.find(name);
    if (found == items.end()) throw std::invalid_argument("No work item called '" + name + "' in " + instanceId);
    return found->second;
}

std::vector<WorkItem*> WorkflowInstance::getItems() const {
    std::vector<WorkItem*> ordered;
    for (std::size_t i = 0; i < itemOrder.size(); ++i) ordered.push_back(items.find(itemOrder[i])->second);
    return ordered;
}

std::vector<std::string> WorkflowInstance::getAvailableWork() const { return tracker->availableItems(); }
const ProgressionTracker& WorkflowInstance::getProgress() const { return *tracker; }

void WorkflowInstance::runCommand(Command* command) {
    const std::string description = command->describe();
    commandLog.run(command);   // owns it; deletes it and rethrows if it is refused
    history.record(description);
}

WorkflowComponent* WorkflowInstance::guard(WorkItem* item, Participant* actor) {
    if (actor == nullptr) throw std::invalid_argument("An action needs an actor");
    guards.push_back(std::unique_ptr<AccessControlProxy>(
        new AccessControlProxy(workOf(item), &definition->getAccessPolicy(), *actor, item->getRequiredRole())));
    return guards.back().get();
}

void WorkflowInstance::beginAction() {
    routedLast = false;
    lastProgression = ProgressionResult();
}

// Turns the tracker's routing result into WorkItem state changes.
void WorkflowInstance::apply(const ProgressionResult& result) {
    for (std::size_t i = 0; i < result.reopened.size(); ++i) getItem(result.reopened[i])->reopen();
    for (std::size_t i = 0; i < result.madeAvailable.size(); ++i) open(result.madeAvailable[i]);
    if (!result.decision.description.empty()) history.record("Routing: " + result.decision.description);
    if (result.instanceComplete) history.record("Instance " + instanceId + " complete");
    lastProgression = result;
    routedLast = true;
}

void WorkflowInstance::open(const std::string& name) {
    getItem(name)->makeAvailable();
    decorate(name);
}

void WorkflowInstance::decorate(const std::string& name) {
    std::map<std::string, std::unique_ptr<Decorated> >::iterator old = decorated.find(name);
    if (old != decorated.end()) {
        retired.push_back(std::move(old->second));
        decorated.erase(old);
    }
    const WorkflowDefinition::DecorationRule* rule = definition->getDecoration(name);
    if (rule == nullptr) return;
    std::unique_ptr<Decorated> layers(new Decorated());
    layers->outer = getItem(name);
    if (rule->deadline > 0) {
        DeadlineDecorator* deadline = new DeadlineDecorator(layers->outer, rule->deadline, clock);
        layers->layers.push_back(std::unique_ptr<TaskDecorator>(deadline));
        layers->deadline = deadline;
        layers->outer = deadline;
    }
    if (rule->audit) {
        layers->layers.push_back(std::unique_ptr<TaskDecorator>(new AuditDecorator(layers->outer, &history)));
        layers->outer = layers->layers.back().get();
    }
    decorated[name] = std::move(layers);
}

WorkflowComponent* WorkflowInstance::workOf(WorkItem* item) const {
    std::map<std::string, std::unique_ptr<Decorated> >::const_iterator found = decorated.find(item->getName());
    return found == decorated.end() ? item : found->second->outer;
}

void WorkflowInstance::setField(const std::string& key, const std::string& value) {
    KeyValuePayload* fields = dynamic_cast<KeyValuePayload*>(payload.get());
    if (fields != nullptr) fields->set(key, value);
}

// Bug 1: routing only sees the instance data, so how an item finished must be in it.
// Written after the command succeeds and before routing; the payload is in the undo snapshot.
void WorkflowInstance::recordOutcome(const std::string& item, const std::string& outcome) {
    setField(RoutingCondition::outcomeField(item), outcome);
}

void WorkflowInstance::assignWork(const std::string& name, Participant* actor, Participant* assignee) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    runCommand(new AssignCommand(item, actor, assignee, guard(item, actor)));
}

void WorkflowInstance::startWork(const std::string& name, Participant* actor) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    runCommand(new StartCommand(item, actor, guard(item, actor)));
    if (needsApproval(name)) submitApproval(name, actor);
}

void WorkflowInstance::completeWork(const std::string& name, Participant* actor) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    if (needsApproval(name)) {
        // Decided by the approval chain. After an undo the decision may already exist.
        const ApprovalRequest* request = latestApproval(name);
        if (request != nullptr && request->getStatus() == PENDING)
            throw std::logic_error("'" + name + "' is waiting on approval " + approvalId(request->getId()) + ": approve or reject it");
        if (request == nullptr || request->getStatus() != APPROVED || item->getCurrentState()->getStateName() != "Started")
            throw std::logic_error("'" + name + "' is decided through the approval chain: start it to request approval");
        guard(item, actor)->authorize("complete");
        finishApproved(name);
        return;
    }
    WorkflowComponent* target = guard(item, actor);
    const WorkflowDefinition::ConnectorBinding* binding = definition->getConnectorBinding(name);
    // Adapter: only call the external system when the completion itself is allowed.
    if (binding != nullptr) target->authorize("complete");
    if (binding != nullptr && item->getCurrentState()->getStateName() == "Started" &&
        item->getAssignedParticipant() == actor) {
        std::unique_ptr<DataPayload> response = binding->connector->invoke(binding->operation, *payload);
        if (!response) throw std::runtime_error("Connector for '" + name + "' returned no data");
        payload.reset(response.release());
        history.record("Connector: '" + name + "' called " + binding->operation);
    }
    runCommand(new CompleteCommand(item, actor, target));
    recordOutcome(name, "completed");
    apply(tracker->complete(name, *payload));
}

void WorkflowInstance::rejectWork(const std::string& name, Participant* actor, const std::string& reason) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    runCommand(new RejectCommand(item, actor, reason, guard(item, actor)));
    recordOutcome(name, "rejected");
    apply(tracker->reject(name, *payload));
}

void WorkflowInstance::cancelWork(const std::string& name, Participant* actor) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    runCommand(new CancelCommand(item, actor, guard(item, actor)));
    tracker->cancel(name);
    if (tracker->isComplete()) history.record("Instance " + instanceId + " complete");
}

void WorkflowInstance::escalateWork(const std::string& name, Participant* actor) {
    requireStarted();
    beginAction();
    WorkItem* item = getItem(name);
    runCommand(new EscalateCommand(item, actor, guard(item, actor)));
}

// ---- Chain of Responsibility ----

bool WorkflowInstance::needsApproval(const std::string& name) const {
    return approvalDesk && definition->getApprovalRule(name) != nullptr;
}

const ApprovalRequest* WorkflowInstance::latestApproval(const std::string& name) const {
    if (!approvalDesk) return nullptr;
    const std::vector<const ApprovalRequest*> all = approvalDesk->all();
    for (std::size_t i = all.size(); i > 0; --i)
        if (all[i - 1]->getItemName() == name) return all[i - 1];
    return nullptr;
}

void WorkflowInstance::submitApproval(const std::string& name, Participant* requester) {
    if (approvalDesk->hasPending(name)) return;   // e.g. started again after an undo
    const WorkflowDefinition::ApprovalRule* rule = definition->getApprovalRule(name);
    double value = 0;
    const std::map<std::string, std::string> fields = getDataFields();
    std::map<std::string, std::string>::const_iterator found = fields.find(rule->valueField);
    if (found != fields.end()) {
        try { value = std::stod(found->second); }
        catch (const std::exception&) { throw std::invalid_argument(rule->valueField + " must be a number to request approval"); }
    }
    const ApprovalRequest* request = approvalDesk->submit(name, requester, value, "Approval for " + name);
    if (request->getStatus() == APPROVED) finishApproved(name);
    else if (request->getStatus() != PENDING) finishRejected(name, &system, request->getDecisionNote());
}

void WorkflowInstance::finishApproved(const std::string& name) {
    WorkItem* item = getItem(name);
    setField(definition->getApprovalRule(name)->resultField, "yes");
    runCommand(new CompleteCommand(item, item->getAssignedParticipant(), workOf(item)));
    recordOutcome(name, "completed");
    apply(tracker->complete(name, *payload));
}

void WorkflowInstance::finishRejected(const std::string& name, Participant* decider, const std::string& reason) {
    WorkItem* item = getItem(name);
    setField(definition->getApprovalRule(name)->resultField, "no");
    runCommand(new RejectCommand(item, decider, reason));
    recordOutcome(name, "rejected");
        recordOutcome(name, "rejected");
    apply(tracker->reject(name, *payload));
}

void WorkflowInstance::approve(int requestId, Participant* actor, const std::string& note) {
    requireStarted();
    beginAction();
    if (!approvalDesk) throw std::logic_error(instanceId + " has no approvals");
    if (actor == nullptr) throw std::invalid_argument("An approval needs an actor");
    const ApprovalRequest* request = nullptr;
    try { request = approvalDesk->get(requestId); }
    catch (const std::out_of_range&) { throw std::invalid_argument("No approval " + approvalId(requestId) + " in " + instanceId); }
    if (getItem(request->getItemName())->getCurrentState()->getStateName() != "Started")
        throw std::logic_error("'" + request->getItemName() + "' is not waiting on a decision");
    approvalDesk->decide(requestId, *actor, true, note);
    finishApproved(request->getItemName());
}

void WorkflowInstance::rejectApproval(int requestId, Participant* actor, const std::string& reason) {
    requireStarted();
    beginAction();
    if (!approvalDesk) throw std::logic_error(instanceId + " has no approvals");
    if (actor == nullptr) throw std::invalid_argument("An approval needs an actor");
    if (reason.empty()) throw std::invalid_argument("A rejection needs a reason");
    const ApprovalRequest* request = nullptr;
    try { request = approvalDesk->get(requestId); }
    catch (const std::out_of_range&) { throw std::invalid_argument("No approval " + approvalId(requestId) + " in " + instanceId); }
    if (getItem(request->getItemName())->getCurrentState()->getStateName() != "Started")
        throw std::logic_error("'" + request->getItemName() + "' is not waiting on a decision");
    approvalDesk->decide(requestId, *actor, false, reason);
    finishRejected(request->getItemName(), actor, reason);
}

std::vector<const ApprovalRequest*> WorkflowInstance::getApprovals() const {
    return approvalDesk ? approvalDesk->all() : std::vector<const ApprovalRequest*>();
}

// ---- clock ----

std::vector<std::string> WorkflowInstance::onClockAdvanced() {
    beginAction();
    std::vector<std::string> changes;
    if (approvalDesk) {
        const std::vector<const ApprovalRequest*> moved = approvalDesk->checkDeadlines();
        for (std::size_t i = 0; i < moved.size(); ++i) {
            const ApprovalRequest* request = moved[i];
            if (request->getStatus() == PENDING) {
                changes.push_back(instanceId + ": " + approvalId(request->getId()) + " escalated to " + request->getHandler()->getName());
            } else if (getItem(request->getItemName())->getCurrentState()->getStateName() == "Started") {
                finishRejected(request->getItemName(), &system, request->getDecisionNote());
                changes.push_back(instanceId + ": " + approvalId(request->getId()) + " " + statusName(request->getStatus()) +
                                  ", '" + request->getItemName() + "' rejected");
            }
        }
    }
    for (std::size_t i = 0; i < itemOrder.size(); ++i) {
        const std::string& name = itemOrder[i];
        std::map<std::string, std::unique_ptr<Decorated> >::const_iterator found = decorated.find(name);
        if (found == decorated.end() || found->second->deadline == nullptr || !found->second->deadline->isOverdue()) continue;
        WorkItem* item = getItem(name);
        // Bug 4: overdue work that someone already holds (Assigned or Started) is escalated
        // too. Before, only Available work was, so a late Started step could never finish.
        const std::string state = item->getCurrentState()->getStateName();
        if (state != "Available" && state != "Assigned" && state != "Started") continue;
        if (needsApproval(name) && state == "Started") continue;   // the approval chain owns its deadline
        Participant* holder = state == "Available" ? nullptr : item->getAssignedParticipant();
        runCommand(new EscalateCommand(item, &system));
        decorate(name);   // escalated work gets a fresh deadline
        changes.push_back(instanceId + ": '" + name + "' is overdue and was escalated" +
                          (holder ? " (taken from " + holder->getName() + ")" : ""));
    }
    return changes;
}

int WorkflowInstance::getDueTime(const std::string& name) const {
    std::map<std::string, std::unique_ptr<Decorated> >::const_iterator found = decorated.find(name);
    if (found == decorated.end() || found->second->deadline == nullptr) return -1;
    return found->second->deadline->getDueTime();
}

void WorkflowInstance::raise(const Event& event) {
    requireStarted();
    beginAction();
    notifyListeners(event);
}

void WorkflowInstance::interrupt(const WorkflowDefinition::InterruptRule& rule, const std::string& reason) {
      requireStarted();
    if (tracker->isComplete()) return; // Events must not restart a finished instance.

    history.record("Interrupt " + rule.eventType + (reason.empty() ? "" : ": " + reason));
    for (std::size_t i = 0; i < rule.cancel.size(); ++i) {
        const std::string& name = rule.cancel[i];
        if (!tracker->isAvailable(name) && !tracker->isWaiting(name)) continue;  // finished or never reached
        WorkItem* item = getItem(name);
        if (isOpen(item->getCurrentState()->getStateName())) runCommand(new CancelCommand(item, &system));
        // Include unreached tasks: they stay Created but routing can never open
        // them after this event. Completed task history is left intact.
        tracker->block(name);
        if (approvalDesk)
            approvalDesk->cancelPending(name, "Cancelled by " + rule.eventType);
    }
    history.record("Interrupt " + rule.eventType + " blocked its cancelled workflow paths");
    for (std::size_t i = 0; i < rule.open.size(); ++i)
        apply(tracker->activate(rule.open[i]));
}

void WorkflowInstance::onEvent(const Event& event) { notifyListeners(event); }

const InstanceHistory& WorkflowInstance::getHistory() const { return history; }
std::vector<std::string> WorkflowInstance::getCommandLog() const { return commandLog.describeAll(); }

void WorkflowInstance::accept(WorkflowVisitor* visitor) {
    if (visitor == nullptr) throw std::invalid_argument("Visitor cannot be null");
    root->accept(visitor);
}

// Memento pattern: creating a snapshot for history and rollback
InstanceSnapshot* WorkflowInstance::createSnapshot() const {
    std::map<std::string, InstanceSnapshot::ItemRecord> saved;
    for (std::map<std::string, WorkItem*>::const_iterator it = items.begin(); it != items.end(); ++it) {
        InstanceSnapshot::ItemRecord record;
        record.state = it->second->getCurrentState()->getStateName();
        record.assignee = it->second->getAssignedParticipant();
        saved[it->first] = record;
    }
    std::map<std::string, int> deadlines;
    for (const auto& entry : decorated)
        deadlines[entry.first] = entry.second->deadline == nullptr
            ? -1 : entry.second->deadline->getDueTime();
    ApprovalDesk::Snapshot approvals;
    if (approvalDesk) approvals = approvalDesk->save();
    return new InstanceSnapshot(saved, payload->clone(), tracker->saveProgress(),
                                approvals, deadlines, commandLog.size(), this);
}

void WorkflowInstance::restore(const InstanceSnapshot* snapshot) {
    if (snapshot == nullptr) throw std::invalid_argument("Cannot restore a null snapshot");
    const std::map<std::string, InstanceSnapshot::ItemRecord>& saved = snapshot->getSavedItems();
    for (std::map<std::string, InstanceSnapshot::ItemRecord>::const_iterator it = saved.begin(); it != saved.end(); ++it) {
        getItem(it->first)->restoreState(createWorkItemState(it->second.state), it->second.assignee);
    }
    payload.reset(snapshot->getSavedPayload() != nullptr ? snapshot->getSavedPayload()->clone() : new KeyValuePayload());
    tracker->restoreProgress(snapshot->getSavedProgress());
    started = snapshot->getSavedProgress().started;

    if (approvalDesk) approvalDesk->restore(snapshot->getSavedApprovals());

    // Logged guards can still point to old layers, so retire rather than delete them.
    for (auto& entry : decorated) retired.push_back(std::move(entry.second));
    decorated.clear();
    for (const auto& entry : snapshot->getSavedDeadlines()) {
        decorate(entry.first);
        Decorated* layers = decorated.at(entry.first).get();
        if (layers->deadline != nullptr) layers->deadline->restoreDueTime(entry.second);
    }
    commandLog.truncate(snapshot->getSavedCommandCount());
    beginAction();
    history.record("Snapshot restored; audit history retained");
}
