#include "DataBasedRouting.h"

#include <stdexcept>

namespace {
std::string joinNames(const std::vector<std::string>& names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out += ", ";
        out += names[i];
    }
    return out;
}

void requireTargets(const std::string& decisionItem, const std::vector<std::string>& targets) {
    if (targets.empty()) {
        throw std::invalid_argument("DataBasedRouting: a branch after '" + decisionItem +
                                    "' must lead to at least one work item");
    }
}
}

void DataBasedRouting::addBranch(const std::string& decisionItem, const RoutingCondition& condition,
                                 const std::vector<std::string>& targets) {
    requireTargets(decisionItem, targets);
    decisions[decisionItem].branches.push_back(Branch(condition, targets));
}

void DataBasedRouting::setDefaultBranch(const std::string& decisionItem,
                                        const std::vector<std::string>& targets) {
    requireTargets(decisionItem, targets);
    decisions[decisionItem].defaultTargets = targets;
}

void DataBasedRouting::setRejectionPath(const std::string& item, const std::vector<std::string>& targets) {
    requireTargets(item, targets);
    rejectionPaths[item] = targets;
}

bool DataBasedRouting::hasRejectionPath(const std::string& item) const {
    return rejectionPaths.find(item) != rejectionPaths.end();
}

bool DataBasedRouting::hasDecision(const std::string& item) const {
    return decisions.find(item) != decisions.end();
}

RoutingDecision DataBasedRouting::determineNextWork(const std::string& completedItem,
                                                    const DataPayload& instanceData) const {
    std::map<std::string, std::vector<std::string> >::const_iterator onReject = rejectionPaths.find(completedItem);
    if (onReject != rejectionPaths.end() && RoutingCondition::rejected(completedItem).evaluate(instanceData)) {
        RoutingDecision decision;
        decision.completedItem = completedItem;
        decision.nextItems = onReject->second;
        decision.description = "Rejected: " + completedItem + " -> " + joinNames(decision.nextItems);
        return decision;
    }

    std::map<std::string, DecisionPoint>::const_iterator point = decisions.find(completedItem);
    if (point == decisions.end()) {
        return followPath(completedItem);
    }

    RoutingDecision decision;
    decision.completedItem = completedItem;

    const std::vector<Branch>& branches = point->second.branches;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        if (branches[i].condition.evaluate(instanceData)) {
            decision.nextItems = branches[i].targets;
            decision.description = "Decision at " + completedItem + ": " +
                                   branches[i].condition.getLabel() + " -> " + joinNames(decision.nextItems);
            return decision;
        }
    }

    if (point->second.defaultTargets.empty()) {
        throw std::runtime_error("DataBasedRouting: no branch after '" + completedItem +
                                 "' matches this instance's data and there is no default branch");
    }
    decision.nextItems = point->second.defaultTargets;
    decision.description = "Decision at " + completedItem + ": [otherwise] -> " + joinNames(decision.nextItems);
    return decision;
}
