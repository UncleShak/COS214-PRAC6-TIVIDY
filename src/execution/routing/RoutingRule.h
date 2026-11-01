#ifndef ROUTING_RULE_H
#define ROUTING_RULE_H

#include <stdexcept>
#include <string>
#include <vector>

#include "DataPayload.h"
#include "KeyValuePayload.h"
#include "WorkflowComponent.h"

/**
 * @file RoutingRule.h
 * @brief Strategy interface: decides which work comes next after a work item finishes.
 */

/// What a RoutingRule decided, by work-item name (WorkItem::getName()).
struct RoutingDecision {
    std::string completedItem;            ///< the item that finished
    std::string description;              ///< readable branch taken, for history / interface
    std::vector<std::string> nextItems;   ///< items to reach next (several = parallel split)
};

/**
 * @brief Strategy (GoF). The definition owns one rule; every instance shares it.
 *
 * Rules work with work-item names so one rule serves every instance; each
 * WorkflowInstance maps the names back to its own WorkItem objects.
 */
class RoutingRule {
public:
    virtual ~RoutingRule() = default;

    /// @throws std::runtime_error if a decision point has no matching branch and no default.
    virtual RoutingDecision determineNextWork(const std::string& completedItem,
                                              const DataPayload& instanceData) const = 0;

    /**
     * Same call in the team's pointer style (class diagram: WorkItem*, DataPayload*).
     * Pass a WorkItem* (or any WorkflowComponent*). A null payload is treated as
     * "no data", matching WorkflowInstance, which allows a null initial payload.
     * @throws std::invalid_argument if completedItem is null.
     */
    RoutingDecision determineNextWork(const WorkflowComponent* completedItem,
                                      const DataPayload* instanceData) const {
        if (completedItem == nullptr) {
            throw std::invalid_argument("RoutingRule: completed work item cannot be null");
        }
        if (instanceData == nullptr) {
            KeyValuePayload noData;
            return determineNextWork(completedItem->getName(), noData);
        }
        return determineNextWork(completedItem->getName(), *instanceData);
    }
};

#endif
