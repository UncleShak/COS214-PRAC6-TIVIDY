#include "SequentialRouting.h"

#include <algorithm>
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
}

void SequentialRouting::addPath(const std::string& from, const std::string& to) {
    if (from.empty() || to.empty()) {
        throw std::invalid_argument("SequentialRouting: work item names may not be empty");
    }
    if (from == to) {
        throw std::invalid_argument("SequentialRouting: '" + from + "' cannot route to itself");
    }
    std::vector<std::string>& successors = configuredPath[from];
    if (std::find(successors.begin(), successors.end(), to) == successors.end()) {
        successors.push_back(to);
    }
}

RoutingDecision SequentialRouting::determineNextWork(const std::string& completedItem,
                                                     const DataPayload& ) const {
    return followPath(completedItem);
}

RoutingDecision SequentialRouting::followPath(const std::string& completedItem) const {
    RoutingDecision decision;
    decision.completedItem = completedItem;

    std::map<std::string, std::vector<std::string> >::const_iterator it =
        configuredPath.find(completedItem);
    if (it == configuredPath.end() || it->second.empty()) {
        decision.description = completedItem + " ends its path";
        return decision;
    }

    decision.nextItems = it->second;
    if (decision.nextItems.size() == 1) {
        decision.description = "Sequence: " + completedItem + " -> " + decision.nextItems[0];
    } else {
        decision.description = "Parallel split: " + completedItem + " -> " + joinNames(decision.nextItems);
    }
    return decision;
}
