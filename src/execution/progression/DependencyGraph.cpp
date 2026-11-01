#include "DependencyGraph.h"

#include <stdexcept>

void DependencyGraph::addDependency(const std::string& dependent, const std::string& prerequisite) {
    if (dependent.empty() || prerequisite.empty()) {
        throw std::invalid_argument("DependencyGraph: work item names may not be empty");
    }
    if (dependent == prerequisite) {
        throw std::invalid_argument("DependencyGraph: '" + dependent + "' cannot depend on itself");
    }

    if (dependsOn(prerequisite, dependent)) {
        throw std::invalid_argument("DependencyGraph: '" + dependent + "' depending on '" +
                                    prerequisite + "' would create a cycle");
    }
    prerequisites[dependent].insert(prerequisite);
}

std::vector<std::string> DependencyGraph::prerequisitesOf(const std::string& item) const {
    std::map<std::string, std::set<std::string> >::const_iterator it = prerequisites.find(item);
    if (it == prerequisites.end()) return std::vector<std::string>();
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> DependencyGraph::unmetPrerequisites(const std::string& item,
                                                             const std::set<std::string>& completed) const {
    std::vector<std::string> unmet;
    std::map<std::string, std::set<std::string> >::const_iterator it = prerequisites.find(item);
    if (it == prerequisites.end()) return unmet;
    for (std::set<std::string>::const_iterator p = it->second.begin(); p != it->second.end(); ++p) {
        if (completed.find(*p) == completed.end()) unmet.push_back(*p);
    }
    return unmet;
}

bool DependencyGraph::isSatisfied(const std::string& item, const std::set<std::string>& completed) const {
    return unmetPrerequisites(item, completed).empty();
}

bool DependencyGraph::dependsOn(const std::string& start, const std::string& target) const {
    std::vector<std::string> toVisit(1, start);
    std::set<std::string> seen;
    while (!toVisit.empty()) {
        const std::string current = toVisit.back();
        toVisit.pop_back();
        if (current == target) return true;
        if (!seen.insert(current).second) continue;
        std::map<std::string, std::set<std::string> >::const_iterator it = prerequisites.find(current);
        if (it == prerequisites.end()) continue;
        toVisit.insert(toVisit.end(), it->second.begin(), it->second.end());
    }
    return false;
}
