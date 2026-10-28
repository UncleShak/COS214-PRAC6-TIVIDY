#ifndef DATA_BASED_ROUTING_H
#define DATA_BASED_ROUTING_H

#include <map>
#include <string>
#include <vector>

#include "RoutingCondition.h"
#include "SequentialRouting.h"

/**
 * @file DataBasedRouting.h
 * @brief ConcreteStrategy: fixed paths plus decision points that branch on the instance's data.
 *
 * Each decision point is an ordered list of guarded branches and an optional
 * default. The first branch whose RoutingCondition holds wins. Items without a
 * decision point follow the fixed path inherited from SequentialRouting.
 */
class DataBasedRouting : public SequentialRouting {
public:
    virtual ~DataBasedRouting() = default;

    using RoutingRule::determineNextWork;   // keep the WorkItem* / DataPayload* overload visible

    void addBranch(const std::string& decisionItem, const RoutingCondition& condition,
                   const std::vector<std::string>& targets);
    void setDefaultBranch(const std::string& decisionItem, const std::vector<std::string>& targets);
    bool hasDecision(const std::string& item) const;

    RoutingDecision determineNextWork(const std::string& completedItem,
                                      const DataPayload& instanceData) const override;

private:
    struct Branch {
        RoutingCondition condition;
        std::vector<std::string> targets;
        Branch(const RoutingCondition& c, const std::vector<std::string>& t) : condition(c), targets(t) {}
    };

    struct DecisionPoint {
        std::vector<Branch> branches;
        std::vector<std::string> defaultTargets;
    };

    std::map<std::string, DecisionPoint> decisions;
};

#endif
