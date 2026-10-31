#include "doctest.h"

#include "ApprovalDesk.h"
#include "ApprovalHandler.h"
#include "ApprovalRequest.h"
#include "AutoApprover.h"
#include "Clock.h"
#include "History.h"
#include "Participant.h"
#include "Role.h"
#include "RoleBasedApprover.h"
#include "WorkflowException.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class RecordingHistory : public History {
public:
    std::vector<std::string> entries;
    void record(const std::string& entry) override {
        entries.push_back(entry);
    }
};

Participant person(const std::string& name, const std::string& roleName) {
    Participant p(name, name);
    p.addRole(Role(roleName));
    return p;
}

// Auto(2000) -> Supervisor(20000, 24) -> Manager(UNLIMITED, 24)
ApprovalHandler* buildFullChain() {
    ApprovalHandler* head = new AutoApprover("Auto", 2000.0);
    head->setNext(new RoleBasedApprover("Supervisor", "supervisor", 20000.0, 24))
        ->setNext(new RoleBasedApprover("Manager", "manager", UNLIMITED, 24));
    return head;
}

// Auto(2000) -> Supervisor(20000, 24); no UNLIMITED level
ApprovalHandler* buildShortChain() {
    ApprovalHandler* head = new AutoApprover("Auto", 2000.0);
    head->setNext(new RoleBasedApprover("Supervisor", "supervisor", 20000.0, 24));
    return head;
}

}  // namespace

TEST_SUITE("wrath") {

TEST_CASE("low value is approved instantly with decider starting with auto:") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 500.0, "small");
    CHECK(r->getStatus() == APPROVED);
    CHECK(r->getDecidedBy().find("auto:") == 0);
}

TEST_CASE("mid value waits at supervisor with deadline now + 24") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(10);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 5000.0, "mid");
    CHECK(r->getStatus() == PENDING);
    CHECK(r->getHandler() != nullptr);
    CHECK(r->getHandler()->getName() == "Supervisor");
    CHECK(r->getDeadline() == 34);
}

TEST_CASE("very high value reaches manager") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 50000.0, "high");
    CHECK(r->getStatus() == PENDING);
    CHECK(r->getHandler() != nullptr);
    CHECK(r->getHandler()->getName() == "Manager");
}

TEST_CASE("value no level can take is REJECTED by (chain)") {
    std::unique_ptr<ApprovalHandler> chain(buildShortChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 50000.0, "too-high");
    CHECK(r->getStatus() == REJECTED);
    CHECK(r->getDecidedBy() == "(chain)");
}

TEST_CASE("escalate moves a pending request to next level with fresh deadline") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    chain->handleRequest(&r, 0);
    CHECK(r.getStatus() == PENDING);
    CHECK(r.getHandler()->getName() == "Supervisor");
    CHECK(r.getDeadline() == 24);

    r.getHandler()->escalate(&r, 100);
    CHECK(r.getStatus() == PENDING);
    CHECK(r.getHandler()->getName() == "Manager");
    CHECK(r.getDeadline() == 124);
}

TEST_CASE("escalate from last level resolves EXPIRED") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ApprovalRequest r(1, "X", "Sam", 50000.0, "high", 0);
    chain->handleRequest(&r, 0);
    CHECK(r.getHandler()->getName() == "Manager");

    r.getHandler()->escalate(&r, 100);
    CHECK(r.getStatus() == EXPIRED);
    CHECK(r.getDecidedBy() == "(chain)");
}

TEST_CASE("mayDecide is true only for the level's own role") {
    RoleBasedApprover sup("Supervisor", "supervisor", 20000.0, 24);
    Participant sam = person("Sam", "supervisor");
    Participant bob = person("Bob", "manager");
    CHECK(sup.mayDecide(sam));
    CHECK_FALSE(sup.mayDecide(bob));
}

TEST_CASE("mayDecide is false at an automatic level") {
    AutoApprover autoApprover("Auto", 2000.0);
    Participant anyone = person("Anyone", "supervisor");
    CHECK_FALSE(autoApprover.mayDecide(anyone));
}

TEST_CASE("resolving twice throws InvalidTransition") {
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    r.resolve(APPROVED, "Sam", "first");
    CHECK_THROWS_AS(r.resolve(REJECTED, "Bob", "second"), InvalidTransition);
}

TEST_CASE("InvalidTransition catches as WorkflowException") {
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    r.resolve(APPROVED, "Sam", "first");
    CHECK_THROWS_AS(r.resolve(REJECTED, "Bob", "second"), WorkflowException);
}

TEST_CASE("InvalidTransition catches as std::logic_error") {
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    r.resolve(APPROVED, "Sam", "first");
    CHECK_THROWS_AS(r.resolve(REJECTED, "Bob", "second"), std::logic_error);
}

TEST_CASE("request is overdue from its deadline onward") {
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    r.assignTo(nullptr, 10);
    CHECK_FALSE(r.isOverdue(9));
    CHECK(r.isOverdue(10));
    CHECK(r.isOverdue(11));
}

TEST_CASE("decided request is never overdue") {
    ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
    r.assignTo(nullptr, 10);
    r.resolve(APPROVED, "Sam", "");
    CHECK_FALSE(r.isOverdue(1000));
}

TEST_CASE("setNext refuses a loop") {
    AutoApprover a("Auto", 2000.0);
    CHECK_THROWS_AS(a.setNext(&a), std::invalid_argument);
}

TEST_CASE("setNext replaces and frees the old successor") {
    // Sanitizer build verifies no leak; this test only confirms the call succeeds.
    std::unique_ptr<ApprovalHandler> chain(new AutoApprover("Auto", 2000.0));
    chain->setNext(new RoleBasedApprover("Supervisor", "supervisor", 20000.0, 24));
    chain->setNext(new RoleBasedApprover("Manager", "manager", UNLIMITED, 24));
    CHECK(chain->getName() == "Auto");
}

TEST_CASE("ids are per desk and each submit is recorded") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* a = desk.submit("X", &sam, 500.0, "small");
    const ApprovalRequest* b = desk.submit("Y", &sam, 500.0, "small");
    CHECK(a->getId() == 1);
    CHECK(b->getId() == 2);
    CHECK(history.entries.size() == 2);
    CHECK(history.entries[0].find("approval #1") == 0);
    CHECK(history.entries[1].find("approval #2") == 0);
}

TEST_CASE("auto-approved request is stored") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 500.0, "small");
    CHECK(r->getStatus() == APPROVED);
    CHECK(desk.get(1) == r);
}

TEST_CASE("second pending request for same item is refused") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    desk.submit("X", &sam, 5000.0, "mid");
    CHECK_THROWS_AS(desk.submit("X", &sam, 5000.0, "mid"), InvalidTransition);
}

TEST_CASE("get unknown id throws out_of_range") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    CHECK_THROWS_AS(desk.get(999), std::out_of_range);
}

TEST_CASE("authorised participant approves and the note and history are recorded") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant bob = person("Bob", "manager");
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 50000.0, "high");
    ApprovalStatus s = desk.decide(r->getId(), bob, true, "looks fine");
    CHECK(s == APPROVED);
    CHECK(desk.get(r->getId())->getStatus() == APPROVED);
    CHECK(desk.get(r->getId())->getDecidedBy() == "Bob");
    CHECK(desk.get(r->getId())->getDecisionNote() == "looks fine");
    CHECK(history.entries.back().find("Approved by Bob") != std::string::npos);
}

TEST_CASE("authorised participant rejects and the note is recorded") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant bob = person("Bob", "manager");
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 50000.0, "high");
    ApprovalStatus s = desk.decide(r->getId(), bob, false, "missing invoice");
    CHECK(s == REJECTED);
    CHECK(desk.get(r->getId())->getDecisionNote() == "missing invoice");
}

TEST_CASE("unauthorised participant gets AccessDenied and the request stays PENDING") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 5000.0, "mid");
    CHECK_THROWS_AS(desk.decide(r->getId(), sam, true, ""), AccessDenied);
    CHECK(desk.get(r->getId())->getStatus() == PENDING);
}

TEST_CASE("decided request can't be decided again") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant bob = person("Bob", "manager");
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 50000.0, "high");
    desk.decide(r->getId(), bob, true, "");
    CHECK_THROWS_AS(desk.decide(r->getId(), bob, false, ""), InvalidTransition);
}

TEST_CASE("nothing happens one unit before the deadline") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    desk.submit("X", &sam, 5000.0, "mid");
    clock.advance(23);
    CHECK(desk.checkDeadlines().empty());
}

TEST_CASE("at the deadline, supervisor to manager, supervisor refused, manager accepted") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");
    Participant bob = person("Bob", "manager");

    const ApprovalRequest* r = desk.submit("X", &sam, 5000.0, "mid");
    clock.advance(24);
    CHECK(desk.checkDeadlines().size() == 1);
    CHECK(desk.get(r->getId())->getHandler()->getName() == "Manager");

    CHECK_THROWS_AS(desk.decide(r->getId(), sam, true, ""), AccessDenied);
    CHECK_NOTHROW(desk.decide(r->getId(), bob, true, ""));
}

TEST_CASE("huge clock jump moves it one level only, then the next deadline expires it") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clock(0);
    RecordingHistory history;
    ApprovalDesk desk(chain.get(), &clock, &history);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* r = desk.submit("X", &sam, 5000.0, "mid");

    clock.advance(1000);
    CHECK(desk.checkDeadlines().size() == 1);
    CHECK(desk.get(r->getId())->getStatus() == PENDING);
    CHECK(desk.get(r->getId())->getHandler()->getName() == "Manager");

    clock.advance(24);
    CHECK(desk.checkDeadlines().size() == 1);
    CHECK(desk.get(r->getId())->getStatus() == EXPIRED);
}

TEST_CASE("two desks sharing one chain both start at id 1") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clockA(0), clockB(0);
    RecordingHistory histA, histB;
    ApprovalDesk deskA(chain.get(), &clockA, &histA);
    ApprovalDesk deskB(chain.get(), &clockB, &histB);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* a = deskA.submit("X", &sam, 500.0, "small");
    const ApprovalRequest* b = deskB.submit("X", &sam, 500.0, "small");
    CHECK(a->getId() == 1);
    CHECK(b->getId() == 1);
}

TEST_CASE("decision on desk A leaves desk B pending") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clockA(0), clockB(0);
    RecordingHistory histA, histB;
    ApprovalDesk deskA(chain.get(), &clockA, &histA);
    ApprovalDesk deskB(chain.get(), &clockB, &histB);
    Participant sam = person("Sam", "supervisor");

    const ApprovalRequest* a = deskA.submit("X", &sam, 5000.0, "mid");
    const ApprovalRequest* b = deskB.submit("X", &sam, 5000.0, "mid");
    deskA.decide(a->getId(), sam, true, "");
    CHECK(deskA.get(a->getId())->getStatus() == APPROVED);
    CHECK(deskB.get(b->getId())->getStatus() == PENDING);
}

TEST_CASE("advancing one desk's clock leaves the other desk alone") {
    std::unique_ptr<ApprovalHandler> chain(buildFullChain());
    ManualClock clockA(0), clockB(0);
    RecordingHistory histA, histB;
    ApprovalDesk deskA(chain.get(), &clockA, &histA);
    ApprovalDesk deskB(chain.get(), &clockB, &histB);
    Participant sam = person("Sam", "supervisor");

    deskA.submit("X", &sam, 5000.0, "mid");
    deskB.submit("X", &sam, 5000.0, "mid");

    clockA.advance(24);
    CHECK(deskA.checkDeadlines().size() == 1);
    CHECK(deskB.checkDeadlines().empty());
}

TEST_CASE("every engine refusal can be caught as WorkflowException") {
    {
        ApprovalRequest r(1, "X", "Sam", 5000.0, "mid", 0);
        r.resolve(APPROVED, "Sam", "");
        CHECK_THROWS_AS(r.resolve(REJECTED, "Bob", ""), WorkflowException);
    }
    {
        std::unique_ptr<ApprovalHandler> chain(buildFullChain());
        ManualClock clock(0);
        RecordingHistory history;
        ApprovalDesk desk(chain.get(), &clock, &history);
        Participant sam = person("Sam", "supervisor");
        const ApprovalRequest* r = desk.submit("X", &sam, 5000.0, "mid");
        CHECK_THROWS_AS(desk.decide(r->getId(), sam, true, ""), WorkflowException);
    }
}

}  // TEST_SUITE