#include "doctest.h"
#include <vector>
#include "Participant.h"
#include "WorkItem.h"
#include "RoleBasedAssignment.h"
#include "WorkloadBasedAssignment.h"

// Small helper so each test reads clearly.
static Participant makeP(const std::string& name, const std::string& role) {
    Participant p(name, name);
    p.addRole(Role(role));
    return p;
}

TEST_CASE("role-based: picks only a participant with the required role") {
    Participant alice = makeP("Alice", "Warehouse");
    Participant bob   = makeP("Bob", "Driver");
    std::vector<Participant*> pool = { &alice, &bob };
    WorkItem deliver("Deliver", "Driver");
    RoleBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == &bob);
}

TEST_CASE("role-based: first eligible participant wins") {
    Participant d1 = makeP("D1", "Driver");
    Participant d2 = makeP("D2", "Driver");
    std::vector<Participant*> pool = { &d1, &d2 };
    WorkItem deliver("Deliver", "Driver");
    RoleBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == &d1);
}

TEST_CASE("role-based: nobody eligible fails safely with nullptr") {
    Participant alice = makeP("Alice", "Warehouse");
    std::vector<Participant*> pool = { &alice };
    WorkItem deliver("Deliver", "Driver");
    RoleBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == nullptr);
}

TEST_CASE("empty pool and null item fail safely") {
    std::vector<Participant*> empty;
    Participant bob = makeP("Bob", "Driver");
    std::vector<Participant*> pool = { &bob };
    WorkItem deliver("Deliver", "Driver");
    RoleBasedAssignment rb;
    WorkloadBasedAssignment wb;
    CHECK(rb.assignParticipant(&deliver, empty) == nullptr);
    CHECK(wb.assignParticipant(&deliver, empty) == nullptr);
    CHECK(rb.assignParticipant(nullptr, pool) == nullptr);
    CHECK(wb.assignParticipant(nullptr, pool) == nullptr);
}

TEST_CASE("one rule serves items that need different roles") {
    Participant alice = makeP("Alice", "Warehouse");
    Participant bob   = makeP("Bob", "Driver");
    std::vector<Participant*> pool = { &alice, &bob };
    WorkItem pack("Prepare package", "Warehouse");
    WorkItem deliver("Deliver", "Driver");
    RoleBasedAssignment rule;
    CHECK(rule.assignParticipant(&pack, pool) == &alice);
    CHECK(rule.assignParticipant(&deliver, pool) == &bob);
}

TEST_CASE("workload-based: picks the least busy eligible participant") {
    Participant d1 = makeP("D1", "Driver");
    Participant d2 = makeP("D2", "Driver");
    d1.workAssigned(); d1.workAssigned();
    d2.workAssigned();
    std::vector<Participant*> pool = { &d1, &d2 };
    WorkItem deliver("Deliver", "Driver");
    WorkloadBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == &d2);
}

TEST_CASE("workload-based: ties go to the first listed") {
    Participant d1 = makeP("D1", "Driver");
    Participant d2 = makeP("D2", "Driver");
    d1.workAssigned();
    d2.workAssigned();
    std::vector<Participant*> pool = { &d1, &d2 };
    WorkItem deliver("Deliver", "Driver");
    WorkloadBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == &d1);
}

TEST_CASE("workload-based: ignores an idle participant without the role") {
    Participant idleWarehouse = makeP("W", "Warehouse");       // workload 0
    Participant busyDriver    = makeP("D", "Driver");
    busyDriver.workAssigned();
    std::vector<Participant*> pool = { &idleWarehouse, &busyDriver };
    WorkItem deliver("Deliver", "Driver");
    WorkloadBasedAssignment rule;
    CHECK(rule.assignParticipant(&deliver, pool) == &busyDriver);
}

TEST_CASE("workload-based: assigning then finishing rebalances") {
    Participant d1 = makeP("D1", "Driver");
    Participant d2 = makeP("D2", "Driver");
    std::vector<Participant*> pool = { &d1, &d2 };
    WorkItem deliver("Deliver", "Driver");
    WorkloadBasedAssignment rule;

    Participant* first = rule.assignParticipant(&deliver, pool);
    first->workAssigned();
    CHECK(first == &d1);
    Participant* second = rule.assignParticipant(&deliver, pool);
    second->workAssigned();
    CHECK(second == &d2);                       // d1 is now busier
    d1.workFinished();
    CHECK(rule.assignParticipant(&deliver, pool) == &d1);   // d1 is free again
}
