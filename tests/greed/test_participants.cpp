#include "doctest.h"
#include "Participant.h"

TEST_CASE("participant has a role after it is added") {
    Participant p("Alice", "P1");
    p.addRole(Role("Warehouse"));
    CHECK(p.hasRole("Warehouse"));
}

TEST_CASE("querying a role the participant does not have returns false") {
    Participant p("Alice", "P1");
    CHECK_FALSE(p.hasRole("Driver"));          // no roles at all
    p.addRole(Role("Warehouse"));
    CHECK_FALSE(p.hasRole("Driver"));          // has a different role
}

TEST_CASE("participant can hold several roles") {
    Participant p("Dan", "P2");
    p.addRole(Role("Driver"));
    p.addRole(Role("Warehouse"));
    CHECK(p.hasRole("Driver"));
    CHECK(p.hasRole("Warehouse"));
}

TEST_CASE("role matching is exact") {
    Role r("Driver");
    CHECK(r.matches("Driver"));
    CHECK_FALSE(r.matches("driver"));
    CHECK_FALSE(r.matches("Drivers"));
}

TEST_CASE("name and id are stored") {
    Participant p("Alice", "P1");
    CHECK(p.getName() == "Alice");
    CHECK(p.getID() == "P1");
}

TEST_CASE("workload starts at zero and follows assign/finish") {
    Participant p("Bob", "P3");
    CHECK(p.getWorkload() == 0);
    p.workAssigned();
    p.workAssigned();
    CHECK(p.getWorkload() == 2);
    p.workFinished();
    CHECK(p.getWorkload() == 1);
}

TEST_CASE("workload never goes below zero") {
    Participant p("Bob", "P3");
    p.workFinished();
    CHECK(p.getWorkload() == 0);
}
