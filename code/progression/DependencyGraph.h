#ifndef DEPENDENCY_GRAPH_H
#define DEPENDENCY_GRAPH_H

#include <map>
#include <set>
#include <string>
#include <vector>

class DependencyGraph {
public:
    void addDependency(const std::string& dependent, const std::string& prerequisite);
    std::vector<std::string> prerequisitesOf(const std::string& item) const;
    std::vector<std::string> unmetPrerequisites(const std::string& item,
                                                const std::set<std::string>& completed) const;
    bool isSatisfied(const std::string& item, const std::set<std::string>& completed) const;

private:
    bool dependsOn(const std::string& start, const std::string& target) const;

    std::map<std::string, std::set<std::string> > prerequisites;
};

#endif
