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
    /// Can never start: a prerequisite was cancelled or rejected, so it would wait forever.
    /// The tracker counts these as cancelled (they were never opened, so they stay Created).
    /// If routing reaches one again later (a retry), it is reopened like any cancelled item.
    std::vector<std::string> dropped;
    bool instanceComplete;                   ///< nothing available or waiting any more
    ProgressionResult() : instanceComplete(false) {}
};

/// Everything the tracker knows about progress, for Memento snapshots (InstanceSnapshot).
struct ProgressionState {
    bool started;
    std::vector<std::string> available;
    std::vector<std::string> waiting;
    std::set<std::string> completed;
    std::set<std::string> rejected;
    std::set<std::string> cancelled;
    std::set<std::string> blocked; // Interrupt-cancelled paths cannot be reopened.
    ProgressionState() : started(false) {}
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
    /// Waiting items that needed it can never start, so they are dropped (see ProgressionResult).
    ProgressionResult cancel(const std::string& item);
    // Interrupt-only cancellation: also prevent future routing into this task.
    // Unlike ordinary cancel(), accepts tasks that have not been reached yet.
    void block(const std::string& item);
    bool isBlocked(const std::string& item) const;

    /// Reaches an item outside the routing rule (an interrupt, e.g. an event opens an evacuation
    /// step). Same rules as routing: waits on unmet prerequisites, reopens finished work.
    /// @throws std::logic_error before start().
    ProgressionResult activate(const std::string& item);

    /// Memento support: copy out and put back the whole progress.
    ProgressionState saveProgress() const;
    void restoreProgress(const ProgressionState& state);

    /// Pointer-style overloads (team convention: WorkItem*, DataPayload*). Null payload = no data.
    /// @throws std::invalid_argument if item is null.
    ProgressionResult complete(const WorkflowComponent* item, const DataPayload* instanceData);
    ProgressionResult reject(const WorkflowComponent* item, const DataPayload* instanceData);
    ProgressionResult cancel(const WorkflowComponent* item);

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
    /// A prerequisite of `item` was cancelled or rejected, so `item` can never become Available.
    bool canNeverStart(const std::string& item) const;
    /// Drops every waiting item that can never start, repeating while drops make more of them.
    void dropUnreachable(ProgressionResult& result);
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

    std::set<std::string> blocked;
};

#endif
