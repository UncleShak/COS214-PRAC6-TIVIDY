/**
 * @file test_dependencies.cpp
 * @brief Envy: dependencies between work items (DependencyGraph).
 */
#include "doctest.h"

#include <set>
#include <stdexcept>
#include <string>

#include "DependencyGraph.h"

TEST_SUITE("dependencies") {

TEST_CASE("an item with no prerequisites is always satisfied") {
    DependencyGraph graph;
    std::set<std::string> none;
    CHECK(graph.isSatisfied("ReceiveOrder", none));
    CHECK(graph.prerequisitesOf("ReceiveOrder").empty());
}

TEST_CASE("an item is blocked until every prerequisite is complete") {
    DependencyGraph graph;
    graph.addDependency("Dispatch", "PreparePackage");
    graph.addDependency("Dispatch", "AssignDriver");

    std::set<std::string> completed;
    CHECK_FALSE(graph.isSatisfied("Dispatch", completed));
    CHECK(graph.unmetPrerequisites("Dispatch", completed).size() == 2);

    completed.insert("PreparePackage");
    CHECK_FALSE(graph.isSatisfied("Dispatch", completed));
    REQUIRE(graph.unmetPrerequisites("Dispatch", completed).size() == 1);
    CHECK(graph.unmetPrerequisites("Dispatch", completed)[0] == "AssignDriver");

    completed.insert("AssignDriver");
    CHECK(graph.isSatisfied("Dispatch", completed));
}

TEST_CASE("an item cannot depend on itself") {
    DependencyGraph graph;
    CHECK_THROWS_AS(graph.addDependency("Dispatch", "Dispatch"), std::invalid_argument);
    CHECK_THROWS_AS(graph.addDependency("", "Dispatch"), std::invalid_argument);
}

TEST_CASE("a dependency that would create a cycle is refused") {
    DependencyGraph graph;
    graph.addDependency("PreparePackage", "ValidateOrder");
    graph.addDependency("Dispatch", "PreparePackage");

    // Direct and indirect cycles both block their items forever.
    CHECK_THROWS_AS(graph.addDependency("ValidateOrder", "PreparePackage"), std::invalid_argument);
    CHECK_THROWS_AS(graph.addDependency("ValidateOrder", "Dispatch"), std::invalid_argument);

    // The refused dependency was not recorded.
    CHECK(graph.prerequisitesOf("ValidateOrder").empty());
}

}
