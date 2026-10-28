#include "ProgressionTracker.h"

#include "KeyValuePayload.h"

#include <algorithm>
#include <stdexcept>

namespace {
bool contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}

void removeFrom(std::vector<std::string>& list, const std::string& item) {
    list.erase(std::remove(list.begin(), list.end(), item), list.end());
}
}

ProgressionTracker::ProgressionTracker(const RoutingRule& routingRule, const DependencyGraph& dependencyGraph,
                                       const std::vector<std::string>& entries)
    : routing(routingRule), dependencies(dependencyGraph), entryItems(entries), started(false) {
    if (entryItems.empty()) {
        throw std::invalid_argument("ProgressionTracker: a workflow needs at least one entry work item");
    }
}

std::vector<std::string> ProgressionTracker::start() {
    if (started) {
        throw std::logic_error("ProgressionTracker: this instance has already started");
    }
    started = true;
    ProgressionResult result;
    for (std::size_t i = 0; i < entryItems.size(); ++i) {
        reach(entryItems[i], result);
    }
    return result.madeAvailable;
}

ProgressionResult ProgressionTracker::complete(const std::string& item, const DataPayload& instanceData) {
    return finish(item, instanceData, false);
}

ProgressionResult ProgressionTracker::reject(const std::string& item, const DataPayload& instanceData) {
    return finish(item, instanceData, true);
}

ProgressionResult ProgressionTracker::finish(const std::string& item, const DataPayload& instanceData,
                                             bool isRejection) {
    const std::string action = isRejection ? "reject" : "complete";
    if (!started) {
        throw std::logic_error("ProgressionTracker: cannot " + action + " '" + item +
                               "' before the instance starts");
    }
    if (!isAvailable(item)) {
        std::string reason = "it has not been reached";
        if (isWaiting(item)) reason = "it is waiting on unmet prerequisites";
        else if (isCompleted(item)) reason = "it is already Completed";
        else if (isRejected(item)) reason = "it is already Rejected";
        else if (isCancelled(item)) reason = "it was Cancelled";
        throw std::logic_error("ProgressionTracker: cannot " + action + " '" + item + "' because " + reason);
    }

    ProgressionResult result;
    result.outcome = isRejection ? "Rejected" : "Completed";
    // Route first: if routing throws, the item stays Available and progress is unchanged.
    result.decision = routing.determineNextWork(item, instanceData);

    removeFrom(available, item);
    if (isRejection) rejected.insert(item);
    else completed.insert(item);

    for (std::size_t i = 0; i < result.decision.nextItems.size(); ++i) {
        reach(result.decision.nextItems[i], result);
    }

    releaseWaiting(result);

    result.instanceComplete = isComplete();
    return result;
}

void ProgressionTracker::cancel(const std::string& item) {
    if (!started) {
        throw std::logic_error("ProgressionTracker: cannot cancel '" + item + "' before the instance starts");
    }
    if (!isAvailable(item) && !isWaiting(item)) {
        throw std::logic_error("ProgressionTracker: cannot cancel '" + item +
                               "' because it is not Available or waiting");
    }
    removeFrom(available, item);
    removeFrom(waiting, item);
    cancelled.insert(item);
}

std::string ProgressionTracker::nameOf(const WorkflowComponent* item) {
    if (item == nullptr) {
        throw std::invalid_argument("ProgressionTracker: work item cannot be null");
    }
    return item->getName();
}

ProgressionResult ProgressionTracker::complete(const WorkflowComponent* item, const DataPayload* instanceData) {
    const std::string name = nameOf(item);
    if (instanceData == nullptr) {
        KeyValuePayload noData;
        return complete(name, noData);
    }
    return complete(name, *instanceData);
}

ProgressionResult ProgressionTracker::reject(const WorkflowComponent* item, const DataPayload* instanceData) {
    const std::string name = nameOf(item);
    if (instanceData == nullptr) {
        KeyValuePayload noData;
        return reject(name, noData);
    }
    return reject(name, *instanceData);
}

void ProgressionTracker::cancel(const WorkflowComponent* item) {
    cancel(nameOf(item));
}

void ProgressionTracker::reach(const std::string& item, ProgressionResult& result) {
    if (isAvailable(item) || isWaiting(item)) {
        return;
    }
    if (isCompleted(item) || isRejected(item) || isCancelled(item)) {
        // Retry loop. The old WorkItem is in a final state, so the instance needs a fresh one.
        completed.erase(item);
        rejected.erase(item);
        cancelled.erase(item);
        result.reopened.push_back(item);
    }
    if (dependencies.isSatisfied(item, completed)) {
        available.push_back(item);
        result.madeAvailable.push_back(item);
    } else {
        waiting.push_back(item);
        result.waiting.push_back(item);
    }
}

void ProgressionTracker::releaseWaiting(ProgressionResult& result) {
    std::vector<std::string> stillWaiting;
    for (std::size_t i = 0; i < waiting.size(); ++i) {
        if (dependencies.isSatisfied(waiting[i], completed)) {
            available.push_back(waiting[i]);
            result.madeAvailable.push_back(waiting[i]);
            removeFrom(result.waiting, waiting[i]);
        } else {
            stillWaiting.push_back(waiting[i]);
        }
    }
    waiting.swap(stillWaiting);
}

bool ProgressionTracker::isAvailable(const std::string& item) const { return contains(available, item); }
bool ProgressionTracker::isWaiting(const std::string& item) const { return contains(waiting, item); }
bool ProgressionTracker::isCompleted(const std::string& item) const { return completed.count(item) > 0; }
bool ProgressionTracker::isRejected(const std::string& item) const { return rejected.count(item) > 0; }
bool ProgressionTracker::isCancelled(const std::string& item) const { return cancelled.count(item) > 0; }

const std::vector<std::string>& ProgressionTracker::availableItems() const { return available; }
const std::vector<std::string>& ProgressionTracker::waitingItems() const { return waiting; }
const std::set<std::string>& ProgressionTracker::completedItems() const { return completed; }
const std::set<std::string>& ProgressionTracker::rejectedItems() const { return rejected; }
const std::set<std::string>& ProgressionTracker::cancelledItems() const { return cancelled; }

std::vector<std::string> ProgressionTracker::unmetPrerequisites(const std::string& item) const {
    return dependencies.unmetPrerequisites(item, completed);
}

bool ProgressionTracker::isStarted() const { return started; }
bool ProgressionTracker::isComplete() const { return started && available.empty() && waiting.empty(); }
bool ProgressionTracker::isStalled() const { return started && available.empty() && !waiting.empty(); }
