#ifndef SEQUENTIAL_ROUTING_H
#define SEQUENTIAL_ROUTING_H

#include <map>
#include <string>
#include <vector>

#include "RoutingRule.h"

/**
 * @file SequentialRouting.h
 * @brief ConcreteStrategy: fixed paths between work items. Several successors = parallel split.
 */
class SequentialRouting : public RoutingRule {
public:
    virtual ~SequentialRouting() = default;

    using RoutingRule::determineNextWork;   // keep the WorkItem* / DataPayload* overload visible

    /// Adds "after 'from' finishes, reach 'to'". Calling it twice for 'from' makes a parallel split.
    void addPath(const std::string& from, const std::string& to);

    RoutingDecision determineNextWork(const std::string& completedItem,
                                      const DataPayload& instanceData) const override;

protected:
    RoutingDecision followPath(const std::string& completedItem) const;

private:
    std::map<std::string, std::vector<std::string> > configuredPath;
};

#endif
