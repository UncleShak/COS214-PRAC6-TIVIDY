#include "FakeEngine.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace tividy {
namespace ui {

namespace {

const char* const kDefinition = "Fake Order Flow";

ParticipantView person(const std::string& id, const std::string& name, const std::string& role1,
                       const std::string& role2 = "") {
    ParticipantView p;
    p.id = id;
    p.name = name;
    p.roles.push_back(role1);
    if (!role2.empty()) {
        p.roles.push_back(role2);
    }
    p.workload = 0;
    return p;
}

bool hasRole(const ParticipantView& p, const std::string& role) {
    for (std::size_t i = 0; i < p.roles.size(); ++i) {
        if (p.roles[i] == role) {
            return true;
        }
    }
    return false;
}

std::string join(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        out += (i == 0 ? "" : ", ") + values[i];
    }
    return out;
}

std::string number(int value) {
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    return text;
}

/** Same wording for every refused transition, like the state classes' exceptions. */
void refuse(const char* verb, const std::string& item, const std::string& state) {
    throw std::logic_error(std::string("cannot ") + verb + " '" + item + "' while it is " + state);
}

bool isOneOf(const std::string& state, const char* a, const char* b = 0, const char* c = 0,
             const char* d = 0) {
    return state == a || (b && state == b) || (c && state == c) || (d && state == d);
}

ParticipantView* findPerson(std::vector<ParticipantView>& people, const std::string& id) {
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (people[i].id == id) {
            return &people[i];
        }
    }
    return 0;
}

} // namespace

FakeEngine::FakeEngine() : nextId_(1), nextApproval_(1), clock_(0), decisionHasDefault_(true) {
    std::vector<ParticipantView>& a = participants_["fake-a"];
    a.push_back(person("P-A1", "Alex", "Clerk"));
    a.push_back(person("P-A2", "Bo", "Manager"));
    a.push_back(person("P-A3", "Cy", "Packer", "Driver"));
    a.push_back(person("P-A4", "Dee", "Clerk", "Packer"));

    std::vector<ParticipantView>& b = participants_["fake-b"];
    b.push_back(person("P-B1", "Eli", "Clerk", "Manager"));
    b.push_back(person("P-B2", "Fay", "Packer"));
    b.push_back(person("P-B3", "Gus", "Driver"));

    prerequisites_["Pack"].insert("Check Stock");
}

std::vector<IndustryInfo> FakeEngine::listIndustries() const {
    std::vector<IndustryInfo> industries;
    IndustryInfo a;
    a.id = "fake-a";
    a.displayName = "Fake Industry A";
    a.description = "Development stand-in. Not demo data.";
    a.definitions.push_back(kDefinition);
    industries.push_back(a);

    IndustryInfo b = a;
    b.id = "fake-b";
    b.displayName = "Fake Industry B";
    industries.push_back(b);
    return industries;
}

std::vector<DataField> FakeEngine::startFields(const std::string& industryId,
                                               const std::string& definitionName) const {
    participantsOf(industryId);
    if (definitionName != kDefinition) {
        throw std::invalid_argument("unknown workflow definition '" + definitionName + "'");
    }
    std::vector<DataField> fields;
    DataField amount = {"amount", "Order amount", true, true, ""};
    DataField destination = {"destination", "Destination", false, true, ""};
    DataField urgent = {"urgent", "Urgent (yes/no)", false, false, "no"};
    fields.push_back(amount);
    fields.push_back(destination);
    fields.push_back(urgent);
    return fields;
}

std::string FakeEngine::startInstance(const std::string& industryId, const std::string& definitionName,
                                      const std::map<std::string, std::string>& initialData) {
    std::vector<DataField> fields = startFields(industryId, definitionName);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].required && initialData.find(fields[i].key) == initialData.end()) {
            throw std::invalid_argument("missing value for '" + fields[i].key + "'");
        }
    }

    char id[16];
    std::snprintf(id, sizeof(id), "FAKE-%04d", nextId_);

    Instance inst;
    inst.id = id;
    inst.industryId = industryId;
    inst.data = initialData;
    const char* names[] = {"Receive", "Validate", "Check Stock", "Approve", "Pack", "Ship"};
    const char* roles[] = {"Clerk", "Clerk", "Packer", "Manager", "Packer", "Driver"};
    for (int i = 0; i < 6; ++i) {
        Item item;
        item.name = names[i];
        item.role = roles[i];
        item.state = "Created";
        inst.items.push_back(item);
    }
    inst.history.push_back("Instance " + inst.id + " started: " + definitionName + " v1");

    ActionOutcome ignored;
    reach(inst, "Receive", ignored);
    inst.history.push_back("Now available: Receive");

    instances_.push_back(inst);
    ++nextId_;
    return inst.id;
}

std::vector<InstanceView> FakeEngine::listInstances(const std::string& industryId) const {
    participantsOf(industryId);
    std::vector<InstanceView> views;
    for (std::size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].industryId == industryId) {
            views.push_back(getInstance(instances_[i].id));
        }
    }
    return views;
}

InstanceView FakeEngine::getInstance(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    InstanceView view;
    view.id = inst.id;
    view.industryId = inst.industryId;
    view.definitionName = kDefinition;
    view.definitionVersion = "1";
    view.complete = isComplete(inst);
    view.stalled = isStalled(inst);
    view.data = inst.data;
    return view;
}

std::vector<WorkItemView> FakeEngine::listWorkItems(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    std::vector<WorkItemView> views;
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        views.push_back(toView(inst, inst.items[i]));
    }
    return views;
}

std::vector<WorkItemView> FakeEngine::listAvailableWork(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    std::vector<WorkItemView> views;
    for (std::size_t i = 0; i < inst.available.size(); ++i) {
        views.push_back(toView(inst, *findItem(inst, inst.available[i])));
    }
    return views;
}

WorkItemView FakeEngine::getWorkItem(const std::string& instanceId, const std::string& workItemId) const {
    const Instance& inst = instance(instanceId);
    const Item* item = findItem(inst, workItemId);
    if (item == 0) {
        throw std::invalid_argument("unknown work item '" + workItemId + "'");
    }
    return toView(inst, *item);
}

std::vector<ParticipantView> FakeEngine::listParticipants(const std::string& industryId) const {
    return participantsOf(industryId);
}

ActionOutcome FakeEngine::act(const ActionRequest& request) {
    // Work on copies and commit only at the end, so a refused action changes nothing.
    Instance inst = instance(request.instanceId);
    std::vector<ParticipantView> people = participantsOf(inst.industryId);

    Item* item = findItem(inst, request.workItemId);
    if (item == 0) {
        throw std::invalid_argument("unknown work item '" + request.workItemId + "'");
    }
    ParticipantView* actor = findPerson(people, request.actorId);
    if (actor == 0) {
        throw std::invalid_argument("unknown participant '" + request.actorId + "'");
    }

    ActionOutcome outcome;
    outcome.workItemId = item->name;
    outcome.previousState = item->state;
    const std::string quoted = "'" + item->name + "'";

    switch (request.action) {
    case WorkAction::Assign: {
        if (!isOneOf(item->state, "Available", "Escalated")) {
            refuse("assign", item->name, item->state);
        }
        ParticipantView* assignee = 0;
        if (request.assigneeId.empty()) {
            for (std::size_t i = 0; i < people.size() && assignee == 0; ++i) {
                if (hasRole(people[i], item->role)) {
                    assignee = &people[i];
                }
            }
            if (assignee == 0) {
                throw std::logic_error("no participant has the role '" + item->role + "' needed for " + quoted);
            }
        } else {
            assignee = findPerson(people, request.assigneeId);
            if (assignee == 0) {
                throw std::invalid_argument("unknown participant '" + request.assigneeId + "'");
            }
        }
        item->state = "Assigned";
        item->assigneeId = assignee->id;
        ++assignee->workload;
        outcome.commandDescription = actor->name + " assigned " + quoted + " to " + assignee->name;
        break;
    }
    case WorkAction::Start:
        if (item->assigneeId != actor->id) {
            throw std::logic_error(actor->name + " is not assigned to " + quoted);
        }
        if (item->state != "Assigned") {
            refuse("start", item->name, item->state);
        }
        item->state = "Started";
        outcome.commandDescription = actor->name + " started " + quoted;
        break;
    case WorkAction::Complete:
        if (item->assigneeId != actor->id) {
            throw std::logic_error(actor->name + " is not assigned to " + quoted);
        }
        if (item->state != "Started") {
            refuse("complete", item->name, item->state);
        }
        completeAndRoute(inst, *item, outcome); // routing may throw: nothing committed yet
        outcome.commandDescription = actor->name + " completed " + quoted;
        break;
    case WorkAction::Reject:
        if (item->state != "Started") {
            refuse("reject", item->name, item->state);
        }
        item->state = "Rejected";
        item->trackerStatus = "rejected";
        removeAvailable(inst, item->name);
        outcome.commandDescription = actor->name + " rejected " + quoted + ": " + request.reason;
        break;
    case WorkAction::Cancel:
        if (!isOneOf(item->state, "Available", "Assigned", "Started", "Escalated")) {
            refuse("cancel", item->name, item->state);
        }
        item->state = "Cancelled";
        item->trackerStatus = "cancelled";
        removeAvailable(inst, item->name);
        outcome.commandDescription = actor->name + " cancelled " + quoted;
        break;
    case WorkAction::Escalate:
        if (item->state != "Available") {
            refuse("escalate", item->name, item->state);
        }
        item->state = "Escalated";
        outcome.commandDescription = actor->name + " escalated " + quoted;
        break;
    }

    // Finishing work frees the assignee (the Command classes call workFinished()).
    if (isOneOf(item->state, "Completed", "Rejected", "Cancelled")) {
        releaseAssignee(*item, people);
    }
    outcome.newState = item->state;
    commit(inst, people, outcome);
    return outcome;
}

std::vector<std::string> FakeEngine::history(const std::string& instanceId) const {
    return instance(instanceId).history;
}

std::string FakeEngine::progressSummary(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    std::map<std::string, int> counts;
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        ++counts[inst.items[i].state];
    }
    int total = static_cast<int>(inst.items.size());
    int counted = total - counts["Cancelled"];
    int percent = counted > 0 ? counts["Completed"] * 100 / counted : 0;

    std::string summary = number(total) + " work items: ";
    bool firstEntry = true;
    for (std::map<std::string, int>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
        if (it->second == 0) {
            continue;
        }
        summary += (firstEntry ? "" : ", ") + number(it->second) + " " + it->first;
        firstEntry = false;
    }
    return summary + " (" + number(percent) + "% complete)";
}

std::vector<std::string> FakeEngine::auditReport(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        const Item& item = inst.items[i];
        std::string holder = item.assigneeId.empty() ? "unassigned" : nameOf(inst.industryId, item.assigneeId);
        lines.push_back(item.name + " | " + item.state + " | " + holder + " | role: " + item.role);
    }
    return lines;
}

std::vector<std::string> FakeEngine::overdueWork(const std::string& instanceId) const {
    const Instance& inst = instance(instanceId);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        if (inst.items[i].state == "Escalated") {
            names.push_back(inst.items[i].name);
        }
    }
    return names;
}

std::vector<ApprovalView> FakeEngine::listApprovals(const std::string& instanceId) const {
    return instance(instanceId).approvals;
}

ActionOutcome FakeEngine::approve(const std::string& instanceId, const std::string& approvalId,
                                  const std::string& actorId) {
    return decide(instanceId, approvalId, actorId, true, "");
}

ActionOutcome FakeEngine::rejectApproval(const std::string& instanceId, const std::string& approvalId,
                                         const std::string& actorId, const std::string& reason) {
    if (reason.empty()) {
        throw std::invalid_argument("a reason is required to reject " + approvalId);
    }
    return decide(instanceId, approvalId, actorId, false, reason);
}

std::vector<std::string> FakeEngine::advanceClock(int units) {
    if (units <= 0) {
        throw std::invalid_argument("the clock can only move forward");
    }
    clock_ += units;

    std::vector<std::string> changes;
    for (std::size_t i = 0; i < instances_.size(); ++i) {
        Instance& inst = instances_[i];
        for (std::size_t j = 0; j < inst.items.size(); ++j) {
            Item& item = inst.items[j];
            // DeadlineDecorator::isOverdue: now > dueTime. Only Available work can escalate.
            if (item.state == "Available" && item.dueAt >= 0 && clock_ > item.dueAt) {
                item.state = "Escalated";
                std::string line = "'" + item.name + "' passed its deadline (t=" + number(item.dueAt) +
                                   ") and was escalated";
                inst.history.push_back("t=" + number(clock_) + ": " + line);
                changes.push_back(inst.id + ": " + line);
            }
        }
    }
    return changes;
}

// ---- helpers --------------------------------------------------------------------------

ActionOutcome FakeEngine::decide(const std::string& instanceId, const std::string& approvalId,
                                 const std::string& actorId, bool approved, const std::string& reason) {
    Instance inst = instance(instanceId);
    std::vector<ParticipantView> people = participantsOf(inst.industryId);

    ApprovalView* approval = 0;
    for (std::size_t i = 0; i < inst.approvals.size(); ++i) {
        if (inst.approvals[i].id == approvalId) {
            approval = &inst.approvals[i];
        }
    }
    if (approval == 0) {
        throw std::invalid_argument("unknown approval '" + approvalId + "'");
    }
    if (approval->status != "Pending") {
        throw std::logic_error("approval " + approvalId + " is already " + approval->status);
    }
    ParticipantView* actor = findPerson(people, actorId);
    if (actor == 0) {
        throw std::invalid_argument("unknown participant '" + actorId + "'");
    }
    if (!hasRole(*actor, approval->approverRole)) {
        throw std::logic_error(actor->name + " cannot decide " + approvalId + ": it needs the " +
                               approval->approverRole + " role");
    }
    Item* item = findItem(inst, approval->workItemId);
    if (!isOneOf(item->state, "Available", "Assigned", "Started", "Escalated")) {
        refuse(approved ? "approve" : "reject", item->name, item->state);
    }

    ActionOutcome outcome;
    outcome.workItemId = item->name;
    outcome.previousState = item->state;
    const std::string quoted = "'" + item->name + "'";

    if (approved) {
        completeAndRoute(inst, *item, outcome); // may throw before anything is committed
        approval->status = "Approved";
        outcome.commandDescription = actor->name + " approved " + approvalId + " for " + quoted;
    } else {
        item->state = "Rejected";
        item->trackerStatus = "rejected";
        removeAvailable(inst, item->name);
        approval->status = "Rejected";
        approval->reason = reason;
        outcome.commandDescription = actor->name + " rejected " + approvalId + " for " + quoted + ": " + reason;
    }
    approval->decidedBy = actor->name;
    releaseAssignee(*item, people);
    outcome.newState = item->state;
    commit(inst, people, outcome);
    return outcome;
}

void FakeEngine::completeAndRoute(Instance& inst, Item& item, ActionOutcome& outcome) {
    Routing routing = route(inst, item.name);
    item.state = "Completed";
    item.trackerStatus = "completed";
    removeAvailable(inst, item.name);
    outcome.routingDescription = routing.description;
    for (std::size_t i = 0; i < routing.next.size(); ++i) {
        reach(inst, routing.next[i], outcome);
    }
    releaseWaiting(inst, outcome);
}

void FakeEngine::commit(Instance& inst, const std::vector<ParticipantView>& people, ActionOutcome& outcome) {
    outcome.instanceComplete = isComplete(inst);

    inst.history.push_back(outcome.commandDescription + "  [" + outcome.previousState + " -> " +
                           outcome.newState + "]");
    if (!outcome.routingDescription.empty()) {
        inst.history.push_back("Routing: " + outcome.routingDescription);
    }
    if (!outcome.madeAvailable.empty()) {
        inst.history.push_back("Now available: " + join(outcome.madeAvailable));
    }
    inst.history.insert(inst.history.end(), inst.notes.begin(), inst.notes.end());
    inst.notes.clear();
    for (std::size_t i = 0; i < outcome.nowWaiting.size(); ++i) {
        inst.history.push_back("Waiting: " + outcome.nowWaiting[i] + " (needs " +
                               join(unmetPrerequisites(inst, outcome.nowWaiting[i])) + ")");
    }
    if (outcome.instanceComplete) {
        inst.history.push_back("Instance " + inst.id + " complete");
    }

    instance(inst.id) = inst;
    participantsOf(inst.industryId) = people;
}

void FakeEngine::releaseAssignee(const Item& item, std::vector<ParticipantView>& people) {
    ParticipantView* assignee = findPerson(people, item.assigneeId);
    if (assignee != 0 && assignee->workload > 0) {
        --assignee->workload;
    }
}

const FakeEngine::Instance& FakeEngine::instance(const std::string& instanceId) const {
    for (std::size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].id == instanceId) {
            return instances_[i];
        }
    }
    throw std::invalid_argument("unknown instance '" + instanceId + "'");
}

FakeEngine::Instance& FakeEngine::instance(const std::string& instanceId) {
    return const_cast<Instance&>(static_cast<const FakeEngine*>(this)->instance(instanceId));
}

FakeEngine::Item* FakeEngine::findItem(Instance& inst, const std::string& name) {
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        if (inst.items[i].name == name) {
            return &inst.items[i];
        }
    }
    return 0;
}

const FakeEngine::Item* FakeEngine::findItem(const Instance& inst, const std::string& name) {
    return findItem(const_cast<Instance&>(inst), name);
}

const std::vector<ParticipantView>& FakeEngine::participantsOf(const std::string& industryId) const {
    std::map<std::string, std::vector<ParticipantView> >::const_iterator it = participants_.find(industryId);
    if (it == participants_.end()) {
        throw std::invalid_argument("unknown industry '" + industryId + "'");
    }
    return it->second;
}

std::vector<ParticipantView>& FakeEngine::participantsOf(const std::string& industryId) {
    return const_cast<std::vector<ParticipantView>&>(
        static_cast<const FakeEngine*>(this)->participantsOf(industryId));
}

std::string FakeEngine::nameOf(const std::string& industryId, const std::string& participantId) const {
    const std::vector<ParticipantView>& people = participantsOf(industryId);
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (people[i].id == participantId) {
            return people[i].name;
        }
    }
    return participantId;
}

FakeEngine::Routing FakeEngine::route(const Instance& inst, const std::string& completed) const {
    Routing r;
    if (completed == "Receive") {
        r.next.push_back("Validate");
        r.next.push_back("Check Stock");
        r.description = "Parallel split: Receive -> Validate, Check Stock";
    } else if (completed == "Validate") {
        std::map<std::string, std::string>::const_iterator amount = inst.data.find("amount");
        double value = amount == inst.data.end() ? 0.0 : std::strtod(amount->second.c_str(), 0);
        if (value > 1000) {
            r.next.push_back("Approve");
            r.description = "Decision at Validate: amount > 1000 -> Approve";
        } else if (decisionHasDefault_) {
            r.next.push_back("Pack");
            r.description = "Decision at Validate: default -> Pack";
        } else {
            throw std::runtime_error("no branch of 'Validate' matches and it has no default");
        }
    } else if (completed == "Approve") {
        r.next.push_back("Pack");
        r.description = "Sequence: Approve -> Pack";
    } else if (completed == "Pack") {
        r.next.push_back("Ship");
        r.description = "Sequence: Pack -> Ship";
    } else {
        r.description = completed + " ends its path";
    }
    return r;
}

std::vector<std::string> FakeEngine::unmetPrerequisites(const Instance& inst, const std::string& item) const {
    std::vector<std::string> unmet;
    std::map<std::string, std::set<std::string> >::const_iterator it = prerequisites_.find(item);
    if (it == prerequisites_.end()) {
        return unmet;
    }
    for (std::set<std::string>::const_iterator p = it->second.begin(); p != it->second.end(); ++p) {
        const Item* prerequisite = findItem(inst, *p);
        if (prerequisite == 0 || prerequisite->trackerStatus != "completed") {
            unmet.push_back(*p);
        }
    }
    return unmet;
}

void FakeEngine::reach(Instance& inst, const std::string& name, ActionOutcome& outcome) {
    Item* item = findItem(inst, name);
    if (item->trackerStatus == "available" || item->trackerStatus == "waiting") {
        return;
    }
    if (!item->trackerStatus.empty()) {
        // Reached again after finishing: the tracker reopens it (retry loop).
        item->assigneeId.clear();
        item->state = "Created";
        outcome.reopened.push_back(name);
    }
    if (unmetPrerequisites(inst, name).empty()) {
        makeAvailable(inst, *item, outcome);
    } else {
        item->trackerStatus = "waiting";
        outcome.nowWaiting.push_back(name);
    }
}

void FakeEngine::releaseWaiting(Instance& inst, ActionOutcome& outcome) {
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        Item& item = inst.items[i];
        if (item.trackerStatus == "waiting" && unmetPrerequisites(inst, item.name).empty()) {
            makeAvailable(inst, item, outcome);
        }
    }
}

void FakeEngine::makeAvailable(Instance& inst, Item& item, ActionOutcome& outcome) {
    item.state = "Available";
    item.trackerStatus = "available";
    item.dueAt = clock_ + kDeadline;
    inst.available.push_back(item.name);
    outcome.madeAvailable.push_back(item.name);

    if (item.name == "Approve") {
        ApprovalView approval;
        approval.id = "APR-" + number(nextApproval_++);
        approval.workItemId = item.name;
        approval.summary = "Order amount " + inst.data["amount"] + " is over 1000";
        approval.approverRole = "Manager";
        approval.status = "Pending";
        inst.approvals.push_back(approval);
        inst.notes.push_back("Approval " + approval.id + " requested from a " + approval.approverRole +
                               ": " + approval.summary);
    }
}

void FakeEngine::removeAvailable(Instance& inst, const std::string& item) {
    for (std::size_t i = 0; i < inst.available.size(); ++i) {
        if (inst.available[i] == item) {
            inst.available.erase(inst.available.begin() + static_cast<long>(i));
            return;
        }
    }
}

bool FakeEngine::isComplete(const Instance& inst) {
    if (!inst.available.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < inst.items.size(); ++i) {
        if (inst.items[i].trackerStatus == "waiting") {
            return false;
        }
    }
    return true;
}

bool FakeEngine::isStalled(const Instance& inst) {
    return inst.available.empty() && !isComplete(inst);
}

WorkItemView FakeEngine::toView(const Instance& inst, const Item& item) const {
    WorkItemView view;
    view.id = item.name;
    view.name = item.name;
    view.state = item.state;
    view.trackerStatus = item.trackerStatus;
    view.requiredRole = item.role;
    view.assigneeName = item.assigneeId.empty() ? "" : nameOf(inst.industryId, item.assigneeId);
    view.dueAt = item.state == "Available" ? item.dueAt : -1;
    if (item.trackerStatus == "waiting") {
        view.waitingOn = unmetPrerequisites(inst, item.name);
    }
    return view;
}

} // namespace ui
} // namespace tividy
