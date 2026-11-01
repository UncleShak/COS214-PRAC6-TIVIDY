/**
 * @file test_engine_robustness.cpp
 * @brief Undo vs side effects, access control, multiple instances and invalid input, against the
 * REAL engine. A failing check documents an engine bug (nothing under src/ was changed).
 */
#include "doctest.h"

#include <climits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "WorkflowEngine.h"
#include "WorkflowException.h"
#include "ArkhamExpressFactory.h"
#include "DuneHarvestFactory.h"
#include "Participant.h"
#include "WorkItem.h"
#include "ApprovalRequest.h"
#include "KeyValuePayload.h"

namespace {

typedef std::vector<std::string> Names;

// Everything observable about an instance plus participant workloads: used to prove "a failed
// action changes nothing" and "undo restores".
std::string fingerprint(WorkflowEngine& e, const std::string& id) {
    std::ostringstream o;
    WorkflowInstance* inst = e.getInstance(id);
    const std::vector<WorkItem*> items = inst->getItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        o << items[i]->getName() << "=" << items[i]->getCurrentState()->getStateName() << "/"
          << (items[i]->getAssignedParticipant() ? items[i]->getAssignedParticipant()->getID() : "-") << ";";
    }
    o << "|avail:";
    const Names a = inst->getAvailableWork();
    for (std::size_t i = 0; i < a.size(); ++i) o << a[i] << ",";
    o << "|wait:";
    const Names& w = inst->getProgress().waitingItems();
    for (std::size_t i = 0; i < w.size(); ++i) o << w[i] << ",";
    o << "|data:";
    const std::map<std::string, std::string> d = inst->getDataFields();
    for (std::map<std::string, std::string>::const_iterator it = d.begin(); it != d.end(); ++it)
        o << it->first << "=" << it->second << ",";
    o << "|appr:" << inst->getApprovals().size();
    for (std::size_t i = 0; i < inst->getApprovals().size(); ++i) o << inst->getApprovals()[i]->getStatus();
    o << "|work:";
    const std::vector<Participant*> p = e.getParticipants();
    for (std::size_t i = 0; i < p.size(); ++i) o << p[i]->getID() << "=" << p[i]->getWorkload() << ",";
    return o.str();
}

std::string state(WorkflowEngine& e, const std::string& id, const std::string& item) {
    return e.getInstance(id)->getItem(item)->getCurrentState()->getStateName();
}

struct Ax {
    WorkflowEngine e;
    ArkhamExpressFactory f;
    std::map<std::string, std::string> who;
    Ax() {
        e.installIndustry(f);
        who["ReceiveOrder"] = who["ValidateOrder"] = who["CloseOrder"] = "ax-clerk";
        who["CheckStock"] = who["RequestStock"] = who["PreparePackage"] = "ax-wh";
        who["SelectDeliveryType"] = who["AssignDriver"] = who["CancelOrder"] = who["HandleStockFailure"] = "ax-dm";
        who["Dispatch"] = who["Deliver"] = who["ReturnToWarehouse"] = "ax-drv";
    }
    std::shared_ptr<WarehouseStub> wh() { return f.getWarehouse(); }
    void begin(const std::string& id, const std::map<std::string, std::string>& over = std::map<std::string, std::string>()) {
        std::unique_ptr<DataPayload> d = f.createInitialData(id);
        KeyValuePayload* kv = dynamic_cast<KeyValuePayload*>(d.get());
        for (std::map<std::string, std::string>::const_iterator it = over.begin(); it != over.end(); ++it) kv->set(it->first, it->second);
        e.startInstance(id, f.getWorkflowName(), d.get());
    }
    void run(const std::string& id, const std::string& item) {
        CHECK_NOTHROW(e.assignWork(id, item, "ax-dm", who[item]));
        CHECK_NOTHROW(e.startWork(id, item, who[item]));
        CHECK_NOTHROW(e.completeWorkItem(id, item, who[item]));
    }
    void run(const std::string& id, const Names& items) { for (std::size_t i = 0; i < items.size(); ++i) run(id, items[i]); }
    // Whole happy path (with the external stock request when noStock).
    void fullRun(const std::string& id, bool stockRequest) {
        if (stockRequest) CHECK_NOTHROW(e.setData(id, "inStock", "no"));
        run(id, Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
        if (stockRequest) run(id, "RequestStock");
        run(id, Names{"PreparePackage", "SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder"});
    }
    void addWh2() { e.addParticipant("Second Warehouse", "ax-wh2", Names(1, "Warehouse")); }
};

struct Du {
    WorkflowEngine e;
    DuneHarvestFactory f;
    Du() { e.installIndustry(f); }
    void begin(const std::string& id, const std::string& yield) {
        std::unique_ptr<DataPayload> d = f.createInitialData(id);
        KeyValuePayload* kv = dynamic_cast<KeyValuePayload*>(d.get());
        if (yield == "<missing>") { KeyValuePayload fresh; fresh.set("runId", id); e.startInstance(id, f.getWorkflowName(), &fresh); return; }
        kv->set("estimatedYieldTonnes", yield);
        e.startInstance(id, f.getWorkflowName(), d.get());
    }
    void toPermit(const std::string& id, const std::string& starter = "du-ops") {
        CHECK_NOTHROW(e.assignWork(id, "SurveyField", "du-ops", "du-surveyor"));
        CHECK_NOTHROW(e.startWork(id, "SurveyField", "du-surveyor"));
        CHECK_NOTHROW(e.completeWorkItem(id, "SurveyField", "du-surveyor"));
        CHECK_NOTHROW(e.assignWork(id, "ApproveHarvestPermit", "du-ops", starter));
    }
};

}  // namespace

// ======================================================================================
// 1. UNDO vs SIDE EFFECTS
// ======================================================================================

TEST_CASE("undo: RequestStock reserves warehouse stock and undo gives it back") {
    Ax a;
    a.begin("AX-1");
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 20);
    a.e.setData("AX-1", "inStock", "no");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
    a.run("AX-1", "RequestStock");
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 18);
    CHECK(a.e.undo("AX-1") == "complete RequestStock");
    CHECK(state(a.e, "AX-1", "RequestStock") == "Started");
    // The order is back to 'stock not requested yet', so the reservation must be released.
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 20);
}

TEST_CASE("undo: a cancelled order does not keep stock reserved") {
    Ax a;
    a.begin("AX-1");
    a.e.setData("AX-1", "inStock", "no");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock"});
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 18);
    a.e.raiseEvent("AX-1", "CUSTOMER_CANCELLED", "changed mind");
    a.e.undo("AX-1");
    a.e.raiseEvent("AX-1", "CUSTOMER_CANCELLED", "changed mind");
    // Customer cancelled the order: the 2 notebooks should not stay locked to a dead order.
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 20);
}

TEST_CASE("undo: every single action is reversed exactly") {
    Ax a;
    a.begin("AX-1");
    const std::string start = fingerprint(a.e, "AX-1");

    SUBCASE("setData") {
        a.e.setData("AX-1", "urgency", "express");
        CHECK(a.e.undo("AX-1") == "set urgency");
        CHECK(fingerprint(a.e, "AX-1") == start);
    }
    SUBCASE("assign") {
        a.e.assignWork("AX-1", "ReceiveOrder", "ax-dm", "ax-clerk");
        CHECK(a.e.getParticipant("ax-clerk")->getWorkload() == 1);
        a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == start);
        CHECK(a.e.getParticipant("ax-clerk")->getWorkload() == 0);
    }
    SUBCASE("start") {
        a.e.assignWork("AX-1", "ReceiveOrder", "ax-dm", "ax-clerk");
        const std::string assigned = fingerprint(a.e, "AX-1");
        a.e.startWork("AX-1", "ReceiveOrder", "ax-clerk");
        a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == assigned);
    }
    SUBCASE("escalate") {
        a.e.escalateWork("AX-1", "ReceiveOrder", "ax-clerk");
        CHECK(state(a.e, "AX-1", "ReceiveOrder") == "Escalated");
        a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == start);
    }
    SUBCASE("event") {
        a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
        const std::string before = fingerprint(a.e, "AX-1");
        a.e.raiseEvent("AX-1", "CUSTOMER_CANCELLED", "x");
        CHECK(state(a.e, "AX-1", "CheckStock") == "Cancelled");
        a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == before);
        CHECK(state(a.e, "AX-1", "CancelOrder") == "Created");
    }
    SUBCASE("complete (with routing)") {
        a.run("AX-1", "ReceiveOrder");
        const std::string before = fingerprint(a.e, "AX-1");
        a.e.assignWork("AX-1", "ValidateOrder", "ax-dm", "ax-clerk");
        a.e.startWork("AX-1", "ValidateOrder", "ax-clerk");
        const std::string started = fingerprint(a.e, "AX-1");
        a.e.completeWorkItem("AX-1", "ValidateOrder", "ax-clerk");
        a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == started);
        a.e.undo("AX-1"); a.e.undo("AX-1");
        CHECK(fingerprint(a.e, "AX-1") == before);
    }
}

TEST_CASE("undo: advancing the clock is not a per-instance user action") {
    Ax a;
    a.begin("AX-1");
    a.e.advanceClock(1);
    a.e.advanceClock(1);
    a.e.advanceClock(1);
    // Nothing the user did is undoable; three 'clock +1' pseudo entries must not exist.
    CHECK_FALSE(a.e.canUndo("AX-1"));
}

TEST_CASE("undo: one undo after a clock tick still undoes the user's last real action") {
    Ax a;
    a.begin("AX-1");
    a.e.setData("AX-1", "urgency", "express");
    a.e.advanceClock(1);
    // The user presses undo once: expects their setData to go, not an invisible clock tick.
    CHECK(a.e.nextUndoLabel("AX-1") == "set urgency");
    a.e.undo("AX-1");
    CHECK(a.e.getInstance("AX-1")->getDataFields().at("urgency") == "standard");
}

TEST_CASE("undo: undoing an overdue escalation leaves a workable item and a sane clock") {
    Ax a;
    a.begin("AX-1");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "PreparePackage", "SelectDeliveryType", "AssignDriver", "Dispatch"});
    CHECK(state(a.e, "AX-1", "Deliver") == "Available");
    a.e.advanceClock(11);
    CHECK(state(a.e, "AX-1", "Deliver") == "Escalated");
    // Undo the tick(s) the user can see: item is Available again but with the OLD due time (10)
    // while the engine clock stays at 11, so it is overdue the moment it is restored.
    while (a.e.canUndo("AX-1") && state(a.e, "AX-1", "Deliver") == "Escalated") a.e.undo("AX-1");
    CHECK(state(a.e, "AX-1", "Deliver") == "Available");
    CHECK_NOTHROW(a.e.assignWork("AX-1", "Deliver", "ax-dm", "ax-drv"));
    CHECK_NOTHROW(a.e.startWork("AX-1", "Deliver", "ax-drv"));
    CHECK_NOTHROW(a.e.completeWorkItem("AX-1", "Deliver", "ax-drv"));
}

TEST_CASE("undo: nothing to undo") {
    Ax a;
    a.begin("AX-1");
    const std::string before = fingerprint(a.e, "AX-1");
    CHECK_FALSE(a.e.canUndo("AX-1"));
    CHECK_THROWS_AS(a.e.undo("AX-1"), std::logic_error);
    CHECK_THROWS_AS(a.e.nextUndoLabel("AX-1"), std::logic_error);
    CHECK(fingerprint(a.e, "AX-1") == before);
    CHECK_THROWS_AS(a.e.undo("NOPE"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.canUndo("NOPE"), std::invalid_argument);
}

TEST_CASE("undo: a refused action leaves no undo entry and does not consume an earlier one") {
    Ax a;
    a.begin("AX-1");
    a.e.setData("AX-1", "urgency", "express");
    CHECK_THROWS(a.e.startWork("AX-1", "ReceiveOrder", "ax-clerk"));   // not assigned
    CHECK_THROWS(a.e.completeWorkItem("AX-1", "CheckStock", "ax-wh")); // not reached
    CHECK(a.e.nextUndoLabel("AX-1") == "set urgency");
}

TEST_CASE("undo: all the way back to the start, then the run can be redone to completion") {
    for (int stockRequest = 0; stockRequest < 2; ++stockRequest) {
        INFO("with RequestStock: " << stockRequest);
        Ax a;
        a.begin("AX-1");
        const std::string start = fingerprint(a.e, "AX-1");
        a.fullRun("AX-1", stockRequest == 1);
        CHECK(a.e.getInstance("AX-1")->isComplete());
        int undone = 0;
        while (a.e.canUndo("AX-1") && undone < 200) { a.e.undo("AX-1"); ++undone; }
        CHECK(undone > 20);
        CHECK(fingerprint(a.e, "AX-1") == start);
        CHECK_FALSE(a.e.getInstance("AX-1")->isComplete());
        CHECK(a.wh()->remaining("AX-NOTEBOOK") == 20);   // external effect must be reversed too
        a.fullRun("AX-1", stockRequest == 1);
        CHECK(a.e.getInstance("AX-1")->isComplete());
        CHECK_FALSE(a.e.getInstance("AX-1")->isStalled());
        CHECK(a.wh()->remaining("AX-NOTEBOOK") == (stockRequest ? 18 : 20));
    }
}

TEST_CASE("undo: Dune run back to the start and redone, approvals included") {
    Du d;
    d.begin("RUN-1", "40");
    const std::string start = fingerprint(d.e, "RUN-1");
    d.toPermit("RUN-1");
    CHECK_NOTHROW(d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops"));
    CHECK(d.e.getInstance("RUN-1")->getApprovals().size() == 1);
    CHECK_NOTHROW(d.e.approve("RUN-1", 1, "du-ops"));
    while (d.e.canUndo("RUN-1")) d.e.undo("RUN-1");
    CHECK(fingerprint(d.e, "RUN-1") == start);
    CHECK(d.e.getInstance("RUN-1")->getApprovals().empty());
    d.toPermit("RUN-1");
    CHECK_NOTHROW(d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops"));
    CHECK_NOTHROW(d.e.approve("RUN-1", 1, "du-ops"));
    CHECK(state(d.e, "RUN-1", "ApproveHarvestPermit") == "Completed");
}

// ======================================================================================
// 2. ACCESS CONTROL
// ======================================================================================

TEST_CASE("access: unknown actor id is refused by every action and changes nothing") {
    Ax a;
    a.begin("AX-1");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ax-wh");
    const std::string before = fingerprint(a.e, "AX-1");
    CHECK_THROWS_AS(a.e.assignWork("AX-1", "CheckStock", "ghost", "ax-wh"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.autoAssign("AX-1", "CheckStock", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.startWork("AX-1", "CheckStock", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.completeWorkItem("AX-1", "CheckStock", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.rejectWork("AX-1", "CheckStock", "ghost", "r"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.cancelWork("AX-1", "CheckStock", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.escalateWork("AX-1", "CheckStock", "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.approve("AX-1", 1, "ghost"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.rejectApproval("AX-1", 1, "ghost", "r"), std::invalid_argument);
    CHECK_THROWS(a.e.assignWork("AX-1", "CheckStock", "", ""));
    CHECK(fingerprint(a.e, "AX-1") == before);
}

TEST_CASE("access: wrong role cannot assign, start, complete, reject, escalate or cancel") {
    Ax a;
    a.begin("AX-1");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    const std::string avail = fingerprint(a.e, "AX-1");
    // CheckStock needs Warehouse. A Clerk and a Driver must not drive it.
    CHECK_THROWS_AS(a.e.assignWork("AX-1", "CheckStock", "ax-clerk", "ax-wh"), AccessDenied);
    CHECK_THROWS_AS(a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ax-clerk"), AccessDenied);  // assignee lacks role
    CHECK_THROWS_AS(a.e.escalateWork("AX-1", "CheckStock", "ax-drv"), AccessDenied);
    CHECK_THROWS_AS(a.e.cancelWork("AX-1", "CheckStock", "ax-wh"), AccessDenied);              // cancel: DeliveryManager only
    CHECK_THROWS_AS(a.e.cancelWork("AX-1", "CheckStock", "ax-clerk"), AccessDenied);
    CHECK_THROWS(a.e.autoAssign("AX-1", "CheckStock", "ax-drv"));
    CHECK(fingerprint(a.e, "AX-1") == avail);
    a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ax-wh");
    const std::string assigned = fingerprint(a.e, "AX-1");
    CHECK_THROWS(a.e.startWork("AX-1", "CheckStock", "ax-clerk"));
    CHECK_THROWS(a.e.startWork("AX-1", "CheckStock", "ax-dm"));          // manager, but not the assignee
    CHECK(fingerprint(a.e, "AX-1") == assigned);
    a.e.startWork("AX-1", "CheckStock", "ax-wh");
    const std::string started = fingerprint(a.e, "AX-1");
    CHECK_THROWS(a.e.completeWorkItem("AX-1", "CheckStock", "ax-clerk"));
    CHECK_THROWS(a.e.completeWorkItem("AX-1", "CheckStock", "ax-dm"));   // non-assignee manager
    CHECK_THROWS(a.e.rejectWork("AX-1", "CheckStock", "ax-clerk", "no"));
    CHECK(fingerprint(a.e, "AX-1") == started);
}

TEST_CASE("access: a colleague with the same role cannot touch work assigned to someone else") {
    Ax a;
    a.addWh2();
    a.begin("AX-1");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ax-wh");
    a.e.startWork("AX-1", "CheckStock", "ax-wh");
    const std::string started = fingerprint(a.e, "AX-1");
    CHECK_THROWS(a.e.completeWorkItem("AX-1", "CheckStock", "ax-wh2"));
    CHECK_THROWS(a.e.startWork("AX-1", "CheckStock", "ax-wh2"));
    // Rejecting is also an act of the assignee: ax-wh2 must not be able to reject ax-wh's work.
    CHECK_THROWS(a.e.rejectWork("AX-1", "CheckStock", "ax-wh2", "sabotage"));
    CHECK(fingerprint(a.e, "AX-1") == started);
}

TEST_CASE("access: a Manager from another industry has no authority over Arkham work") {
    // RealEngine installs every industry in ONE engine; Dune's du-ops holds the role 'Manager',
    // which the default RoleAccessPolicy treats as an admin override on every instance.
    Ax a;
    DuneHarvestFactory dune;
    a.e.installIndustry(dune);
    a.begin("AX-1");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    const std::string before = fingerprint(a.e, "AX-1");
    SUBCASE("assign") {
        CHECK_THROWS_AS(a.e.assignWork("AX-1", "CheckStock", "du-ops", "ax-wh"), AccessDenied);
        CHECK(fingerprint(a.e, "AX-1") == before);
    }
    SUBCASE("escalate") {
        CHECK_THROWS_AS(a.e.escalateWork("AX-1", "CheckStock", "du-ops"), AccessDenied);
        CHECK(fingerprint(a.e, "AX-1") == before);
    }
    SUBCASE("reject someone else's started work") {
        a.e.assignWork("AX-1", "CheckStock", "ax-dm", "ax-wh");
        a.e.startWork("AX-1", "CheckStock", "ax-wh");
        CHECK_THROWS_AS(a.e.rejectWork("AX-1", "CheckStock", "du-ops", "meddling"), AccessDenied);
    }
}

TEST_CASE("access: Dune approvals respect the level that holds the request") {
    Du d;
    d.begin("RUN-1", "500");   // beyond Operations (100): House management (role Manager)
    d.e.addParticipant("Junior Ops", "du-ops2", Names(1, "OperationsManager"));
    d.toPermit("RUN-1", "du-ops2");
    CHECK_NOTHROW(d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops2"));
    const ApprovalRequest* r = d.e.getInstance("RUN-1")->getApprovals().at(0);
    CHECK(r->getHandler()->getRole() == "Manager");
    const std::string before = fingerprint(d.e, "RUN-1");
    CHECK_THROWS_AS(d.e.approve("RUN-1", 1, "du-ops2"), AccessDenied);          // below the level
    CHECK_THROWS_AS(d.e.approve("RUN-1", 1, "du-surveyor"), AccessDenied);
    CHECK_THROWS_AS(d.e.rejectApproval("RUN-1", 1, "du-pilot", "no"), AccessDenied);
    CHECK_THROWS(d.e.approve("RUN-1", 99, "du-ops"));
    CHECK(fingerprint(d.e, "RUN-1") == before);
    CHECK_NOTHROW(d.e.approve("RUN-1", 1, "du-ops"));
    CHECK_THROWS(d.e.approve("RUN-1", 1, "du-ops"));                              // twice
    CHECK_THROWS(d.e.rejectApproval("RUN-1", 1, "du-ops", "too late"));
}

TEST_CASE("audit: a refused action does not write to the audit history") {
    Ax a;
    a.begin("AX-1");
    const std::size_t before = a.e.getInstance("AX-1")->getHistory().size();
    CHECK_THROWS(a.e.startWork("AX-1", "ReceiveOrder", "ax-clerk"));
    CHECK_THROWS(a.e.completeWorkItem("AX-1", "ReceiveOrder", "ax-clerk"));
    CHECK_THROWS(a.e.raiseEvent("AX-1", "", "x"));
    CHECK(a.e.getInstance("AX-1")->getHistory().size() == before);
}

TEST_CASE("access: nobody approves their own request (separation of duties)") {
    Du d;
    d.begin("RUN-1", "40");
    d.toPermit("RUN-1", "du-ops");
    CHECK_NOTHROW(d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops"));   // du-ops raises the request
    CHECK(d.e.getInstance("RUN-1")->getApprovals().at(0)->getRequesterName() == "Sela Orrin");
    // ... and, being the only OperationsManager, decides it too.
    CHECK_THROWS(d.e.approve("RUN-1", 1, "du-ops"));
}

TEST_CASE("access: approval actions are refused on an instance with no approvals") {
    Ax a;
    a.begin("AX-1");
    CHECK_THROWS_AS(a.e.approve("AX-1", 1, "ax-dm"), std::logic_error);
    CHECK_THROWS_AS(a.e.rejectApproval("AX-1", 1, "ax-dm", "x"), std::logic_error);
}

TEST_CASE("access: data and events are mutable only while the instance is live") {
    Ax a;
    a.begin("AX-1");
    a.fullRun("AX-1", false);
    REQUIRE(a.e.getInstance("AX-1")->isComplete());
    // A closed order's record (and an audited delivery result) must not be editable.
    CHECK_THROWS(a.e.setData("AX-1", "delivered", "no"));
    CHECK_THROWS(a.e.setData("AX-1", "orderId", "AX-999"));
}

// ======================================================================================
// 3. MULTIPLE INSTANCES
// ======================================================================================

TEST_CASE("multi: two orders for 2 of 3 notebooks: first reserves, second is told out of stock") {
    Ax a;
    a.wh()->setStock("AX-NOTEBOOK", 3);
    a.begin("AX-1");
    a.begin("AX-2");
    a.e.setData("AX-1", "inStock", "no");
    a.e.setData("AX-2", "inStock", "no");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock"});
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 1);
    const std::string b0 = fingerprint(a.e, "AX-2");
    a.run("AX-2", Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock"});
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 1);
    CHECK(a.e.getInstance("AX-1")->getDataFields().at("stockReceived") == "yes");
    CHECK(a.e.getInstance("AX-2")->getDataFields().at("stockReceived") == "no");
    CHECK(state(a.e, "AX-1", "PreparePackage") == "Available");
    CHECK(state(a.e, "AX-2", "PreparePackage") == "Created");
    CHECK(state(a.e, "AX-2", "HandleStockFailure") == "Available");
    CHECK(state(a.e, "AX-1", "HandleStockFailure") == "Created");
}

TEST_CASE("multi: a user-editable orderId cannot be used to take over another order's reservation") {
    Ax a;
    a.wh()->setStock("AX-NOTEBOOK", 2);
    a.begin("AX-1");
    a.begin("AX-2");
    a.e.setData("AX-1", "inStock", "no");
    a.e.setData("AX-2", "inStock", "no");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock"});
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 0);
    // AX-2 claims AX-1's order id: the warehouse's idempotency returns AX-1's reservation.
    CHECK_THROWS(a.e.setData("AX-2", "orderId", "AX-1"));
    a.run("AX-2", Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
    a.e.assignWork("AX-2", "RequestStock", "ax-dm", "ax-wh");
    a.e.startWork("AX-2", "RequestStock", "ax-wh");
    a.e.completeWorkItem("AX-2", "RequestStock", "ax-wh");
    CHECK(a.e.getInstance("AX-2")->getDataFields().at("stockReceived") == "no");   // there is no stock left for AX-2
}

TEST_CASE("multi: actions on one instance never touch another") {
    Ax a;
    a.begin("AX-1");
    a.begin("AX-2");
    const std::string b0 = fingerprint(a.e, "AX-2");
    a.e.setData("AX-1", "urgency", "express");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    a.e.raiseEvent("AX-1", "CUSTOMER_CANCELLED", "x");
    a.e.escalateWork("AX-1", "CancelOrder", "ax-dm");
    // Workload is global by design, so compare instance state only.
    std::string b1 = fingerprint(a.e, "AX-2");
    b1 = b1.substr(0, b1.find("|work:"));
    CHECK(b1 == b0.substr(0, b0.find("|work:")));
    CHECK(a.e.getInstance("AX-2")->getDataFields().at("urgency") == "standard");
    CHECK(a.e.canUndo("AX-1"));
    CHECK_FALSE(a.e.canUndo("AX-2"));
}

TEST_CASE("multi: autoAssign balances across instances and undo on one releases only its own load") {
    Ax a;
    a.addWh2();
    a.begin("AX-1");
    a.begin("AX-2");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder"});
    a.run("AX-2", Names{"ReceiveOrder", "ValidateOrder"});
    const std::string first = a.e.autoAssign("AX-1", "CheckStock", "ax-dm");
    const std::string second = a.e.autoAssign("AX-2", "CheckStock", "ax-dm");
    CHECK(first != second);                                   // the second order goes to the idle warehouse hand
    CHECK(a.e.getParticipant(first)->getWorkload() == 1);
    CHECK(a.e.getParticipant(second)->getWorkload() == 1);
    a.e.undo("AX-1");
    CHECK(a.e.getParticipant(first)->getWorkload() == 0);
    CHECK(a.e.getParticipant(second)->getWorkload() == 1);
    CHECK(state(a.e, "AX-2", "CheckStock") == "Assigned");
    CHECK(state(a.e, "AX-1", "CheckStock") == "Available");
    // Third assignment (instance 1 again) must pick the now idle one, which is `first`.
    CHECK(a.e.autoAssign("AX-1", "CheckStock", "ax-dm") == first);
}

TEST_CASE("multi: same instance id twice, generated ids and installIndustry twice") {
    Ax a;
    a.begin("AX-1");
    a.run("AX-1", "ReceiveOrder");
    const std::string before = fingerprint(a.e, "AX-1");
    std::unique_ptr<DataPayload> d = a.f.createInitialData("AX-1");
    CHECK_THROWS_AS(a.e.startInstance("AX-1", "ArkhamExpress", d.get()), std::invalid_argument);
    CHECK(fingerprint(a.e, "AX-1") == before);
    CHECK(a.e.listInstances().size() == 1);

    // The generated id may collide with an id the caller already took.
    a.e.startInstance("ArkhamExpress-1", "ArkhamExpress", d.get());
    WorkflowInstance* gen = nullptr;
    CHECK_NOTHROW(gen = a.e.startInstance("", "ArkhamExpress", d.get()));
    REQUIRE(gen != nullptr);
    CHECK(gen->getId() != "ArkhamExpress-1");
    CHECK(a.e.listInstances().size() == 3);

    const std::size_t people = a.e.getParticipants().size();
    CHECK_THROWS_AS(a.e.installIndustry(a.f), std::invalid_argument);
    CHECK(a.e.getParticipants().size() == people);
    CHECK(a.e.listDefinitions().size() == 1);
}

TEST_CASE("multi: installIndustry is all or nothing when a participant id is already taken") {
    WorkflowEngine e;
    ArkhamExpressFactory f;
    e.addParticipant("Squatter", "ax-wh", Names(1, "Anything"));
    CHECK_THROWS(e.installIndustry(f));
    // Failed install: no half-deployed workflow whose staff is only partly present.
    CHECK(e.listDefinitions().empty());
    // ... so it can be retried once the clash is solved (here: another engine state).
    CHECK(e.getParticipants().size() == 1);
}

// ======================================================================================
// 4. INVALID INPUT
// ======================================================================================

TEST_CASE("input: unknown instance, item, definition and empty names give clean errors") {
    Ax a;
    a.begin("AX-1");
    const std::string before = fingerprint(a.e, "AX-1");
    CHECK_THROWS_AS(a.e.assignWork("NOPE", "ReceiveOrder", "ax-dm", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.autoAssign("NOPE", "ReceiveOrder", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.startWork("NOPE", "ReceiveOrder", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.completeWorkItem("NOPE", "ReceiveOrder", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.rejectWork("NOPE", "ReceiveOrder", "ax-clerk", "r"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.cancelWork("NOPE", "ReceiveOrder", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.escalateWork("NOPE", "ReceiveOrder", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.setData("NOPE", "k", "v"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.raiseEvent("NOPE", "CUSTOMER_CANCELLED", ""), std::invalid_argument);
    CHECK_THROWS_AS(a.e.approve("NOPE", 1, "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.getAvailableWork("NOPE"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.assignWork("", "ReceiveOrder", "ax-dm", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.assignWork("AX-1", "NoSuchItem", "ax-dm", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.autoAssign("AX-1", "NoSuchItem", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.startWork("AX-1", "", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.completeWorkItem("AX-1", "NoSuchItem", "ax-clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.rejectWork("AX-1", "NoSuchItem", "ax-clerk", "r"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.cancelWork("AX-1", "NoSuchItem", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.escalateWork("AX-1", "NoSuchItem", "ax-dm"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.startInstance("X", "NoSuchWorkflow", nullptr), std::invalid_argument);
    CHECK_THROWS_AS(a.e.startInstance("X", "", nullptr), std::invalid_argument);
    CHECK_THROWS_AS(a.e.addParticipant("", "x", Names()), std::invalid_argument);
    CHECK_THROWS_AS(a.e.addParticipant("n", "", Names()), std::invalid_argument);
    CHECK_THROWS_AS(a.e.addParticipant("n", "ax-dm", Names()), std::invalid_argument);
    CHECK_THROWS_AS(a.e.setData("AX-1", "", "v"), std::invalid_argument);
    CHECK_THROWS_AS(a.e.raiseEvent("AX-1", "", "d"), std::invalid_argument);
    CHECK(fingerprint(a.e, "AX-1") == before);
    CHECK_FALSE(a.e.canUndo("AX-1"));   // refused actions leave no undo entry
}

TEST_CASE("input: raising an event the industry does not know is refused") {
    Ax a;
    a.begin("AX-1");
    CHECK_THROWS(a.e.raiseEvent("AX-1", "WORM_SIGN", "wrong industry"));
    CHECK_THROWS(a.e.raiseEvent("AX-1", "STATE_CHANGED", "forged"));
    CHECK_THROWS(a.e.raiseEvent("AX-1", "NO_SUCH_EVENT", "x"));
    CHECK_FALSE(a.e.canUndo("AX-1"));   // a no-op event is not an undoable action
}

TEST_CASE("input: Dune estimatedYieldTonnes must be a sane number to request approval") {
    const char* bad[] = {"abc", "", "12abc", "-5", "nan", "inf", "1e999", "  ", "0x10"};
    for (std::size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        SUBCASE(bad[i]) {
            Du d;
            d.begin("RUN-1", bad[i]);
            d.toPermit("RUN-1");
            const std::string before = fingerprint(d.e, "RUN-1");
            INFO("estimatedYieldTonnes='" << std::string(bad[i]) << "'");
            CHECK_THROWS(d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops"));
            CHECK(fingerprint(d.e, "RUN-1") == before);       // and it rolled back cleanly
            CHECK(state(d.e, "RUN-1", "ApproveHarvestPermit") == "Assigned");
        }
    }
}

TEST_CASE("input: a missing estimatedYieldTonnes must not be an automatic approval") {
    Du d;
    d.begin("RUN-1", "<missing>");
    d.toPermit("RUN-1");
    try { d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops"); } catch (const std::exception&) {}
    // Auto-approver: the permit is granted without anyone ever stating a yield.
    CHECK(state(d.e, "RUN-1", "ApproveHarvestPermit") != "Completed");
    CHECK(d.e.getInstance("RUN-1")->getDataFields().count("permitGranted") == 0);
}

TEST_CASE("input: Arkham quantity problems leave RequestStock started and stock untouched") {
    const char* bad[] = {"0", "-1", "abc", "", "2.5", "99999999999", " 2", "2 "};
    for (std::size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        SUBCASE(bad[i]) {
            Ax a;
            a.begin("AX-1");
            a.e.setData("AX-1", "inStock", "no");
            a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
            a.e.setData("AX-1", "quantity", bad[i]);
            a.e.assignWork("AX-1", "RequestStock", "ax-dm", "ax-wh");
            a.e.startWork("AX-1", "RequestStock", "ax-wh");
            const std::string before = fingerprint(a.e, "AX-1");
            INFO("quantity='" << std::string(bad[i]) << "'");
            CHECK_THROWS(a.e.completeWorkItem("AX-1", "RequestStock", "ax-wh"));
            CHECK(fingerprint(a.e, "AX-1") == before);
            CHECK(state(a.e, "AX-1", "RequestStock") == "Started");
            CHECK(a.wh()->remaining("AX-NOTEBOOK") == 20);
        }
    }
}

TEST_CASE("input: missing data keys fail cleanly and roll back") {
    Ax a;
    KeyValuePayload bare;
    a.e.startInstance("AX-1", "ArkhamExpress", &bare);
    a.e.startInstance("AX-2", "ArkhamExpress", nullptr);
    const char* ids[] = {"AX-1", "AX-2"};
    for (int k = 0; k < 2; ++k) {
        const std::string id = ids[k];
        a.run(id, "ReceiveOrder");
        CHECK_NOTHROW(a.run(id, "ValidateOrder"));     // routing falls to the default branch: CancelOrder
        CHECK(state(a.e, id, "CancelOrder") == "Available");
        CHECK(state(a.e, id, "CheckStock") == "Created");
        CHECK_NOTHROW(a.run(id, "CancelOrder"));
        CHECK(a.e.getInstance(id)->isComplete());
    }
    // The stock request needs orderId, sku and quantity; missing ones refuse and roll back.
    a.e.startInstance("AX-3", "ArkhamExpress", &bare);
    a.e.setData("AX-3", "orderValid", "yes");
    a.run("AX-3", Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
    CHECK(state(a.e, "AX-3", "RequestStock") == "Available");  // inStock missing -> default branch
    a.e.assignWork("AX-3", "RequestStock", "ax-dm", "ax-wh");
    a.e.startWork("AX-3", "RequestStock", "ax-wh");
    const std::string before = fingerprint(a.e, "AX-3");
    CHECK_THROWS_AS(a.e.completeWorkItem("AX-3", "RequestStock", "ax-wh"), std::invalid_argument);
    CHECK(fingerprint(a.e, "AX-3") == before);
    CHECK(a.wh()->reservationCalls() == 0);
}

TEST_CASE("input: warehouse offline refuses the stock request and rolls back") {
    Ax a;
    a.begin("AX-1");
    a.e.setData("AX-1", "inStock", "no");
    a.run("AX-1", Names{"ReceiveOrder", "ValidateOrder", "CheckStock"});
    a.e.assignWork("AX-1", "RequestStock", "ax-dm", "ax-wh");
    a.e.startWork("AX-1", "RequestStock", "ax-wh");
    const std::string before = fingerprint(a.e, "AX-1");
    a.wh()->setOnline(false);
    CHECK_THROWS_AS(a.e.completeWorkItem("AX-1", "RequestStock", "ax-wh"), std::runtime_error);
    CHECK(fingerprint(a.e, "AX-1") == before);
    a.wh()->setOnline(true);
    CHECK_NOTHROW(a.e.completeWorkItem("AX-1", "RequestStock", "ax-wh"));   // and the retry works
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 18);
}

TEST_CASE("input: clock rejects zero, negatives and overflow") {
    Ax a;
    a.begin("AX-1");
    CHECK_THROWS_AS(a.e.advanceClock(0), std::invalid_argument);
    CHECK_THROWS_AS(a.e.advanceClock(-3), std::invalid_argument);
    CHECK(a.e.currentTime() == 0);
    a.e.advanceClock(5);
    // INT_MAX more would overflow the signed clock (undefined behaviour; time would run backwards).
    try { a.e.advanceClock(INT_MAX); } catch (const std::exception&) {}
    CHECK(a.e.currentTime() >= 5);
    try { a.e.advanceClock(1); } catch (const std::exception&) {}
    CHECK(a.e.currentTime() >= 5);
}

TEST_CASE("input: approval ids that do not exist") {
    Du d;
    d.begin("RUN-1", "40");
    d.toPermit("RUN-1");
    d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops");
    const std::string before = fingerprint(d.e, "RUN-1");
    CHECK_THROWS_AS(d.e.approve("RUN-1", 0, "du-ops"), std::invalid_argument);
    CHECK_THROWS_AS(d.e.approve("RUN-1", -1, "du-ops"), std::invalid_argument);
    CHECK_THROWS_AS(d.e.approve("RUN-1", INT_MAX, "du-ops"), std::invalid_argument);
    CHECK_THROWS_AS(d.e.rejectApproval("RUN-1", 7, "du-ops", "x"), std::invalid_argument);
    CHECK_THROWS_AS(d.e.rejectApproval("RUN-1", 1, "du-ops", ""), std::invalid_argument);
    CHECK(fingerprint(d.e, "RUN-1") == before);
}

TEST_CASE("input: actions after the instance is finished are refused") {
    Ax a;
    a.begin("AX-1");
    a.fullRun("AX-1", false);
    REQUIRE(a.e.getInstance("AX-1")->isComplete());
    const std::string before = fingerprint(a.e, "AX-1");
    CHECK_THROWS(a.e.assignWork("AX-1", "CloseOrder", "ax-dm", "ax-clerk"));
    CHECK_THROWS(a.e.cancelWork("AX-1", "CloseOrder", "ax-dm"));
    CHECK_THROWS(a.e.escalateWork("AX-1", "ReceiveOrder", "ax-clerk"));
    CHECK_THROWS(a.e.rejectWork("AX-1", "CloseOrder", "ax-clerk", "r"));
    CHECK(fingerprint(a.e, "AX-1") == before);
}

// ======================================================================================
// 7. WITHDRAWN WORK, WORKLOAD ACCOUNTING AND A SEEDED RANDOM WALK
// ======================================================================================

TEST_CASE("approval: cancelling the work item withdraws its pending approval") {
    Du d;
    d.begin("RUN-1", "40");
    d.toPermit("RUN-1");
    d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops");
    REQUIRE(d.e.getInstance("RUN-1")->getApprovals().at(0)->getStatus() == PENDING);
    d.e.cancelWork("RUN-1", "ApproveHarvestPermit", "du-ops");
    CHECK(state(d.e, "RUN-1", "ApproveHarvestPermit") == "Cancelled");
    // The request is still PENDING for a dead item: it would show as waiting for a decision
    // forever and escalate on the clock.
    CHECK(d.e.getInstance("RUN-1")->getApprovals().at(0)->getStatus() != PENDING);
    const std::vector<std::string> moved = d.e.advanceClock(10);
    CHECK(moved.empty());
}

TEST_CASE("workload: an interrupt that cancels assigned work frees its holder, and undo takes it back") {
    Du d;
    d.begin("RUN-1", "5");
    d.toPermit("RUN-1");
    d.e.startWork("RUN-1", "ApproveHarvestPermit", "du-ops");     // auto approved
    d.e.assignWork("RUN-1", "DeployHarvester", "du-ops", "du-chief");
    d.e.assignWork("RUN-1", "DeployCarryall", "du-ops", "du-pilot");
    d.e.startWork("RUN-1", "DeployCarryall", "du-pilot");
    CHECK(d.e.getParticipant("du-chief")->getWorkload() == 1);
    CHECK(d.e.getParticipant("du-pilot")->getWorkload() == 1);
    d.e.raiseEvent("RUN-1", "WORM_SIGN", "x");
    CHECK(d.e.getParticipant("du-chief")->getWorkload() == 0);
    CHECK(d.e.getParticipant("du-pilot")->getWorkload() == 0);
    d.e.undo("RUN-1");
    CHECK(d.e.getParticipant("du-chief")->getWorkload() == 1);
    CHECK(d.e.getParticipant("du-pilot")->getWorkload() == 1);
}

namespace {

struct Rng {
    unsigned long long s;
    explicit Rng(unsigned long long seed) : s(seed) {}
    unsigned next(unsigned n) { s = s * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<unsigned>((s >> 33) % n); }
};

// Workload of each participant must equal the Assigned/Started items it holds across instances.
std::string workloadMismatch(WorkflowEngine& e) {
    std::map<std::string, int> held;
    const std::vector<std::string> ids = e.listInstances();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const std::vector<WorkItem*> items = e.getInstance(ids[i])->getItems();
        for (std::size_t k = 0; k < items.size(); ++k) {
            const std::string s = items[k]->getCurrentState()->getStateName();
            if (items[k]->getAssignedParticipant() && (s == "Assigned" || s == "Started"))
                ++held[items[k]->getAssignedParticipant()->getID()];
        }
    }
    const std::vector<Participant*> p = e.getParticipants();
    for (std::size_t i = 0; i < p.size(); ++i)
        if (p[i]->getWorkload() != held[p[i]->getID()])
            return p[i]->getID() + " workload " + std::to_string(p[i]->getWorkload()) + " but holds " + std::to_string(held[p[i]->getID()]);
    return "";
}

}  // namespace

TEST_CASE("random walk: failed actions change nothing, undo restores exactly, workloads stay true") {
    const unsigned long long seeds[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    for (std::size_t sd = 0; sd < sizeof(seeds) / sizeof(seeds[0]); ++sd) {
        SUBCASE(std::to_string(seeds[sd]).c_str()) {
            Rng rng(seeds[sd]);
            WorkflowEngine e;
            ArkhamExpressFactory af;
            DuneHarvestFactory df;
            e.installIndustry(af);
            e.installIndustry(df);
            e.addParticipant("Second Warehouse", "ax-wh2", Names(1, "Warehouse"));
            const char* ax[] = {"AX-1", "AX-2"};
            const char* du[] = {"RUN-1", "RUN-2"};
            std::vector<std::string> ids;
            for (int i = 0; i < 2; ++i) {
                std::unique_ptr<DataPayload> a = af.createInitialData(ax[i]);
                e.startInstance(ax[i], "ArkhamExpress", a.get());
                std::unique_ptr<DataPayload> d = df.createInitialData(du[i]);
                e.startInstance(du[i], "DuneSpiceHarvest", d.get());
                ids.push_back(ax[i]); ids.push_back(du[i]);
            }
            const char* axItems[] = {"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock", "PreparePackage", "SelectDeliveryType",
                                     "AssignDriver", "Dispatch", "Deliver", "CloseOrder", "CancelOrder", "HandleStockFailure", "ReturnToWarehouse"};
            const char* duItems[] = {"SurveyField", "ApproveHarvestPermit", "AbandonRun", "DeployHarvester", "DeployCarryall", "Harvest",
                                     "TransportToRefinery", "Evacuate"};
            const char* people[] = {"ax-clerk", "ax-wh", "ax-wh2", "ax-dm", "ax-drv", "du-surveyor", "du-chief", "du-pilot", "du-ops"};
            std::map<std::string, std::vector<std::string> > undoFp;   // fingerprints before each undoable action
            for (int step = 0; step < 500; ++step) {
                const std::string id = ids[rng.next(static_cast<unsigned>(ids.size()))];
                const bool isAx = id[0] == 'A';
                const std::string item = isAx ? axItems[rng.next(13)] : duItems[rng.next(8)];
                const std::string who = people[rng.next(9)];
                const std::string other = people[rng.next(9)];
                const unsigned op = rng.next(14);
                std::map<std::string, std::string> before;
                for (std::size_t i = 0; i < ids.size(); ++i) before[ids[i]] = fingerprint(e, ids[i]);
                std::ostringstream what;
                what << "seed " << seeds[sd] << " step " << step << " op " << op << " " << id << " " << item << " " << who << " " << other;
                INFO(what.str());
                bool ok = true;
                bool wasUndo = false;
                try {
                    switch (op) {
                    case 0: case 1: e.assignWork(id, item, who, other); break;
                    case 2: e.autoAssign(id, item, who); break;
                    case 3: case 4: e.startWork(id, item, who); break;
                    case 5: case 6: e.completeWorkItem(id, item, who); break;
                    case 7: e.rejectWork(id, item, who, "r"); break;
                    case 8: e.cancelWork(id, item, who); break;
                    case 9: e.escalateWork(id, item, who); break;
                    case 10: e.setData(id, isAx ? "inStock" : "permitGranted", rng.next(2) ? "yes" : "no"); break;
                    case 11: e.raiseEvent(id, isAx ? "CUSTOMER_CANCELLED" : "WORM_SIGN", "r"); break;
                    case 12: wasUndo = true; if (e.canUndo(id)) { e.undo(id); } else ok = false; break;
                    default: {
                        const std::vector<const ApprovalRequest*> ap = e.getInstance(id)->getApprovals();
                        if (!ap.empty() && rng.next(2)) { const int rid = ap[rng.next(static_cast<unsigned>(ap.size()))]->getId();
                            if (rng.next(2)) e.approve(id, rid, who); else e.rejectApproval(id, rid, who, "no"); }
                        else e.advanceClock(1 + static_cast<int>(rng.next(4)));
                    }
                    }
                } catch (const std::exception&) { ok = false; }
                if (!ok) {
                    for (std::size_t i = 0; i < ids.size(); ++i) CHECK(fingerprint(e, ids[i]) == before[ids[i]]);
                } else if (wasUndo) {
                    std::vector<std::string>& st = undoFp[id];
                    if (!st.empty()) {
                        const std::string now = fingerprint(e, id);
                        CHECK(now.substr(0, now.find("|work:")) == st.back().substr(0, st.back().find("|work:")));   // workload is global
                        st.pop_back();
                    }
                } else if (op == 13) {
                    // approvals / clock: only the clock pushes entries for every instance
                    for (std::size_t i = 0; i < ids.size(); ++i) undoFp[ids[i]].push_back(before[ids[i]]);
                } else {
                    undoFp[id].push_back(before[id]);
                }
                const std::string bad = workloadMismatch(e);
                CHECK(bad == "");
                if (!bad.empty()) break;
            }
        }
    }
}
