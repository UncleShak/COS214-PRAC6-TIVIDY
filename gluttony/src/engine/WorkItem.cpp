#include "WorkItem.h"

#include "Participant.h"
#include "WorkflowException.h"
#include "WorkflowVisitor.h"

std::string statusName(WorkItemStatus status) {
    switch (status) {
        case WorkItemStatus::Created:   return "Created";
        case WorkItemStatus::Available: return "Available";
        case WorkItemStatus::Assigned:  return "Assigned";
        case WorkItemStatus::Started:   return "Started";
        case WorkItemStatus::Completed: return "Completed";
        case WorkItemStatus::Rejected:  return "Rejected";
        case WorkItemStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

WorkItem::WorkItem(const std::string& name, std::function<void()> work)
    : name(name), work(work), status(WorkItemStatus::Created), assignee(nullptr), requestCount(0) {}

void WorkItem::request() {
    ++requestCount;
    if (work) {
        work();
    }
}

void WorkItem::proxyRequest() {
    // The access check lives in the Proxy; once it lets the call through, the work is the same.
    request();
}

void WorkItem::accept(WorkflowVisitor* v) {
    if (v) {
        v->visitWorkItem(this);
    }
}

WorkflowIterator* WorkItem::createIterator() {
    return nullptr;  // A leaf has no children to iterate over.
}

std::string WorkItem::getName() const { return name; }

void WorkItem::makeAvailable() {
    if (status != WorkItemStatus::Created) refuse("make available");
    status = WorkItemStatus::Available;
}

void WorkItem::assign(Participant* newAssignee) {
    if (status != WorkItemStatus::Available || newAssignee == nullptr) refuse("assign");
    assignee = newAssignee;
    status = WorkItemStatus::Assigned;
}

void WorkItem::start(Participant* actor) {
    if (status != WorkItemStatus::Assigned || actor != assignee) refuse("start");
    status = WorkItemStatus::Started;
}

void WorkItem::complete(Participant* actor) {
    if (status != WorkItemStatus::Started || actor != assignee) refuse("complete");
    status = WorkItemStatus::Completed;
}

void WorkItem::reject(Participant* actor, const std::string& reason) {
    if ((status != WorkItemStatus::Assigned && status != WorkItemStatus::Started) || actor == nullptr) {
        refuse("reject");
    }
    rejectionReason = reason;
    status = WorkItemStatus::Rejected;
}

void WorkItem::cancel(Participant* actor) {
    if (isFinished() || actor == nullptr) refuse("cancel");
    status = WorkItemStatus::Cancelled;
}

void WorkItem::refuse(const std::string& action) const {
    throw InvalidTransition("Cannot " + action + " '" + name + "' while it is " + statusName(status));
}

bool WorkItem::isFinished() const {
    return status == WorkItemStatus::Completed || status == WorkItemStatus::Rejected ||
           status == WorkItemStatus::Cancelled;
}
