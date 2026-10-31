#ifndef PROGRESSION_TRACKER_H
#define PROGRESSION_TRACKER_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "DataPayload.h"
#include "DependencyGraph.h"
#include "WorkflowIterator.h"
#include "RoutingRule.h"
#include "WorkflowComponent.h"

/**
 * @file ProgressionTracker.h
 * @brief One per WorkflowInstance: which work is Available, waiting or finished.
 *
 * Mirrors the team's WorkItemState lifecycle by name:
 *  - an item the tracker makes available -> instance calls WorkItem::makeAvailable()
 *    (CreatedState -> AvailableState: "dependencies are met");
 *  - WorkItem::complete() / reject() / cancel() -> instance calls complete() / reject() / cancel() here.
 */

/// What happened when a work item finished. Names are WorkItem::getName().
struct ProgressionResult {
    std::string outcome;                     ///< "Completed" or "Rejected" (WorkItemState::getStateName())
    RoutingDecision decision;                ///< branch the RoutingRule took
    std::vector<std::string> madeAvailable;  ///< call makeAvailable() on these
    std::vector<std::string> waiting;        ///< reached, but prerequisites unmet (stay in CreatedState)
    /// Reached again after they had finished (retry loop). CompletedState, RejectedState and
    /// CancelledState are final, so the instance gives these a fresh WorkItem in CreatedState.
    std::vector<std::string> reopened;
    bool instanceComplete;                   ///< nothing available or waiting any more
    ProgressionResult() : instanceComplete(false) {}
};

class ProgressionTracker {
public:
    /// @throws std::invalid_argument if there are no entry items.
    ProgressionTracker(const RoutingRule& routing, const DependencyGraph& dependencies,
                       const std::vector<std::string>& entryItems);

    /// Reaches the entry items. @return the ones to make Available. @throws std::logic_error if already started.
    std::vector<std::string> start();

    /// Item finished successfully (WorkItem -> CompletedState): route, then release dependants.
    /// @throws std::logic_error if the item is not Available; progress is then unchanged.
    ProgressionResult complete(const std::string& item, const DataPayload& instanceData);
    /// Item finished with outcome "no" (WorkItem -> RejectedState). Routes like complete(), so the
    /// definition's decision decides the recovery path, but a rejected item does NOT satisfy
    /// dependencies on it.
    ProgressionResult reject(const std::string& item, const DataPayload& instanceData);
    /// Item abandoned (WorkItem -> CancelledState): no routing, removed from available/waiting.
    void cancel(const std::string& item);

    /// Pointer-style overloads (team convention: WorkItem*, DataPayload*). Null payload = no data.
    /// @throws std::invalid_argument if item is null.
    ProgressionResult complete(const WorkflowComponent* item, const DataPayload* instanceData);
    ProgressionResult reject(const WorkflowComponent* item, const DataPayload* instanceData);
    void cancel(const WorkflowComponent* item);

    bool isAvailable(const std::string& item) const;
    bool isWaiting(const std::string& item) const;
    bool isCompleted(const std::string& item) const;
    bool isRejected(const std::string& item) const;
    bool isCancelled(const std::string& item) const;
    const std::vector<std::string>& availableItems() const;
    /**
     * Iterator pattern (Aggregate): a new AvailableWorkIterator over the work available now.
     * The iterator keeps its own copy, so completing work while looping is safe.
     */
    std::unique_ptr<WorkflowIterator<std::string> > createIterator() const;
    const std::vector<std::string>& waitingItems() const;
    const std::set<std::string>& completedItems() const;
    const std::set<std::string>& rejectedItems() const;
    const std::set<std::string>& cancelledItems() const;
    /// Why an item is waiting.
    std::vector<std::string> unmetPrerequisites(const std::string& item) const;
    bool isStarted() const;
    /// Nothing available or waiting: the instance has finished.
    bool isComplete() const;
    /// Work is waiting but nothing is available: it can never proceed (escalate).
    bool isStalled() const;

private:
    ProgressionResult finish(const std::string& item, const DataPayload& instanceData, bool rejected);
    void reach(const std::string& item, ProgressionResult& result);
    void releaseWaiting(ProgressionResult& result);
    static std::string nameOf(const WorkflowComponent* item);

    const RoutingRule& routing;
    const DependencyGraph& dependencies;
    std::vector<std::string> entryItems;

    bool started;
    std::vector<std::string> available;
    std::vector<std::string> waiting;
    std::set<std::string> completed;
    std::set<std::string> rejected;
    std::set<std::string> cancelled;
};

#endif
