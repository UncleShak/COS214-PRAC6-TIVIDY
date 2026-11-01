/**
 * @file test_scenarios.cpp
 * @brief End-to-end scenario tests against the REAL engine. Every path must reach a LOGICAL end:
 * instance complete (not stalled), nothing left waiting, a sensible terminal item reached.
 * A failing check here documents an engine/scenario bug.
 */
#include "doctest.h"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/WorkflowEngine.h"
#include "scenarios/arkham/ArkhamExpressFactory.h"
#include "scenarios/dune/DuneHarvestFactory.h"
#include "participants/Participant.h"
#include "workflow/WorkItem.h"
#include "approval/ApprovalRequest.h"

namespace {

typedef std::vector<std::string> Names;

// ---- shared driver ----
struct Scenario {
    WorkflowEngine engine;
    std::string id;
    std::map<std::string, std::string> who;   // item -> assignee
    std::string manager;                      // actor for assign / cancel

    WorkflowInstance* inst() { return engine.getInstance(id); }
    std::string st(const std::string& item) { return inst()->getItem(item)->getCurrentState()->getStateName(); }

    void assign(const std::string& item) { CHECK_NOTHROW(engine.assignWork(id, item, manager, who[item])); }
    void start(const std::string& item) { CHECK_NOTHROW(engine.startWork(id, item, who[item])); }
    void done(const std::string& item) { CHECK_NOTHROW(engine.completeWorkItem(id, item, who[item])); }
    void run(const std::string& item) { assign(item); start(item); done(item); }
    void run(const Names& items) { for (std::size_t i = 0; i < items.size(); ++i) run(items[i]); }
    void data(const std::string& k, const std::string& v) { CHECK_NOTHROW(engine.setData(id, k, v)); }

    // Complete whatever is open (and approvals) until nothing is available. Bounded.
    void drain() {
        for (int round = 0; round < 40; ++round) {
            Names open = inst()->getAvailableWork();
            if (open.empty()) return;
            for (std::size_t i = 0; i < open.size(); ++i) {
                const std::string s = st(open[i]);
                try {
                    if (s == "Available" || s == "Escalated") engine.assignWork(id, open[i], manager, who[open[i]]);
                    if (st(open[i]) == "Assigned") engine.startWork(id, open[i], who[open[i]]);
                    if (st(open[i]) == "Started") {
                        if (!pendingApproval(open[i])) engine.completeWorkItem(id, open[i], who[open[i]]);
                    }
                } catch (const std::exception&) {}
            }
            // nothing moved: give up
            if (inst()->getAvailableWork() == open) {
                bool moved = false;
                for (std::size_t i = 0; i < open.size(); ++i) if (st(open[i]) != "Available") moved = true;
                if (!moved) return;
            }
        }
    }
    virtual bool pendingApproval(const std::string&) { return false; }

    bool waiting() { return !inst()->getProgress().waitingItems().empty(); }
    std::string dump() {
        std::ostringstream o;
        const std::vector<WorkItem*> all = inst()->getItems();
        for (std::size_t i = 0; i < all.size(); ++i) {
            const std::string s = all[i]->getCurrentState()->getStateName();
            if (s != "Created") o << all[i]->getName() << "=" << s << " ";
        }
        o << "| available:";
        Names a = inst()->getAvailableWork();
        for (std::size_t i = 0; i < a.size(); ++i) o << " " << a[i];
        o << " | waiting:";
        const Names& w = inst()->getProgress().waitingItems();
        for (std::size_t i = 0; i < w.size(); ++i) o << " " << w[i];
        o << (inst()->isComplete() ? " | COMPLETE" : "") << (inst()->isStalled() ? " | STALLED" : "");
        return o.str();
    }
    // The logical end: finished, not stalled, nothing waiting or open.
    void logicalEnd() {
        INFO(dump());
        CHECK(inst()->isComplete());
        CHECK_FALSE(inst()->isStalled());
        CHECK_FALSE(waiting());
        CHECK(inst()->getAvailableWork().empty());
    }
    void notStalled() { INFO(dump()); CHECK_FALSE(inst()->isStalled()); }
    void is(const std::string& item, const std::string& state) { INFO(dump()); CHECK(st(item) == state); }
    // The item was reached and finished in one of the terminal states (not Created).
    void reached(const std::string& item) { INFO(dump()); CHECK(st(item) != "Created"); }
    void notReached(const std::string& item) { INFO(dump()); CHECK(st(item) == "Created"); }
};

// ---- Arkham ----
struct Ax : Scenario {
    ArkhamExpressFactory factory;
    Ax() : factory() {
        engine.installIndustry(factory);
        manager = "ax-dm";
        who["ReceiveOrder"] = who["ValidateOrder"] = who["CloseOrder"] = "ax-clerk";
        who["CheckStock"] = who["RequestStock"] = who["PreparePackage"] = "ax-wh";
        who["SelectDeliveryType"] = who["AssignDriver"] = who["CancelOrder"] = who["HandleStockFailure"] = "ax-dm";
        who["Dispatch"] = who["Deliver"] = who["ReturnToWarehouse"] = "ax-drv";
        id = "ORD-1";
        std::unique_ptr<DataPayload> d = factory.createInitialData(id);
        engine.startInstance(id, factory.getWorkflowName(), d.get());
    }
    std::shared_ptr<WarehouseStub> wh() { return factory.getWarehouse(); }
    void toValidated() { run(Names{"ReceiveOrder", "ValidateOrder"}); }
    void toChecked() { toValidated(); run("CheckStock"); }
    void toPrepared() { toChecked(); run("PreparePackage"); }
    void toDispatched() { toPrepared(); run(Names{"SelectDeliveryType", "AssignDriver", "Dispatch"}); }
    // Reach the point where `item` is open (taking the branch that opens it).
    void until(const std::string& item) {
        if (item == "ReceiveOrder") return;
        if (item == "ValidateOrder") { run("ReceiveOrder"); return; }
        if (item == "CancelOrder") { data("orderValid", "no"); toValidated(); return; }
        if (item == "CheckStock") { toValidated(); return; }
        if (item == "RequestStock") { data("inStock", "no"); toChecked(); return; }
        if (item == "HandleStockFailure") { data("inStock", "no"); wh()->setStock("AX-NOTEBOOK", 0); toChecked(); run("RequestStock"); return; }
        if (item == "PreparePackage") { toChecked(); return; }
        if (item == "SelectDeliveryType" || item == "AssignDriver") { toPrepared(); return; }
        if (item == "Dispatch") { toPrepared(); run(Names{"SelectDeliveryType", "AssignDriver"}); return; }
        if (item == "Deliver") { toDispatched(); return; }
        if (item == "CloseOrder") { toDispatched(); run("Deliver"); return; }
        if (item == "ReturnToWarehouse") { data("delivered", "no"); toDispatched(); run("Deliver"); return; }
    }
};

Names axItems() {
    return Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock", "PreparePackage",
                 "SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder",
                 "CancelOrder", "HandleStockFailure", "ReturnToWarehouse"};
}

// ---- Dune ----
struct Du : Scenario {
    DuneHarvestFactory factory;
    Du(const std::string& yield = "40") {
        engine.installIndustry(factory);
        manager = "du-ops";
        who["SurveyField"] = "du-surveyor";
        who["ApproveHarvestPermit"] = who["AbandonRun"] = "du-ops";
        who["DeployHarvester"] = who["Harvest"] = "du-chief";
        who["DeployCarryall"] = who["TransportToRefinery"] = who["Evacuate"] = "du-pilot";
        id = "RUN-1";
        std::unique_ptr<DataPayload> d = factory.createInitialData(id);
        engine.startInstance(id, factory.getWorkflowName(), d.get());
        data("estimatedYieldTonnes", yield);
    }
    bool pendingApproval(const std::string& item) override {
        if (item != "ApproveHarvestPermit") return false;
        return decide(true);
    }
    // Decide the latest pending permit request. @return true if one was pending.
    bool decide(bool yes, const std::string& actor = "du-ops") {
        std::vector<const ApprovalRequest*> r = inst()->getApprovals();
        if (r.empty() || r.back()->getStatus() != PENDING) return false;
        const int rid = r.back()->getId();
        if (yes) CHECK_NOTHROW(engine.approve(id, rid, actor, "ok"));
        else CHECK_NOTHROW(engine.rejectApproval(id, rid, actor, "no"));
        return true;
    }
    void survey() { run("SurveyField"); }
    // Survey then submit the permit request. Returns after the request exists.
    void request() {
        survey();
        assign("ApproveHarvestPermit");
        start("ApproveHarvestPermit");
    }
    void permit(const std::string& actor = "du-ops") { request(); decide(true, actor); }
    void deployed() { permit(); run(Names{"DeployHarvester", "DeployCarryall"}); }
    void harvested() { deployed(); run("Harvest"); }
    void until(const std::string& item) {
        if (item == "SurveyField") return;
        if (item == "ApproveHarvestPermit") { survey(); return; }
        if (item == "AbandonRun") { request(); decide(false); return; }
        if (item == "DeployHarvester" || item == "DeployCarryall") { permit(); return; }
        if (item == "Harvest") { deployed(); return; }
        if (item == "TransportToRefinery") { harvested(); return; }
        if (item == "Evacuate") { permit(); engine.raiseEvent(id, "WORM_SIGN", "worm"); return; }
    }
};

Names duItems() {
    return Names{"SurveyField", "ApproveHarvestPermit", "AbandonRun", "DeployHarvester",
                 "DeployCarryall", "Harvest", "TransportToRefinery", "Evacuate"};
}


}  // namespace

TEST_SUITE("scenario end states") {

// =============================== ARKHAM ===============================

TEST_CASE("Arkham: happy path closes the order") {
    Ax a;
    a.toDispatched();
    a.run("Deliver");
    a.run("CloseOrder");
    a.logicalEnd();
    a.is("CloseOrder", "Completed");
    a.notReached("CancelOrder");
    a.notReached("ReturnToWarehouse");
}

TEST_CASE("Arkham: invalid order is cancelled") {
    Ax a;
    a.data("orderValid", "no");
    a.toValidated();
    a.is("CancelOrder", "Available");
    a.notReached("CheckStock");
    a.run("CancelOrder");
    a.logicalEnd();
    a.is("CancelOrder", "Completed");
}

TEST_CASE("Arkham: out of stock locally, warehouse reserves, order proceeds to close") {
    Ax a;
    a.data("inStock", "no");
    a.toChecked();
    a.is("RequestStock", "Available");
    a.run("RequestStock");
    CHECK(a.wh()->remaining("AX-NOTEBOOK") == 18);
    CHECK(a.inst()->getDataFields().at("stockReceived") == "yes");
    a.is("PreparePackage", "Available");
    a.run("PreparePackage");
    a.run(Names{"SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder"});
    a.logicalEnd();
    a.is("CloseOrder", "Completed");
}

TEST_CASE("Arkham: out of stock everywhere ends with stock failure then cancel") {
    Ax a;
    a.data("inStock", "no");
    a.wh()->setStock("AX-NOTEBOOK", 0);
    a.toChecked();
    a.run("RequestStock");
    CHECK(a.inst()->getDataFields().at("stockReceived") == "no");
    a.is("HandleStockFailure", "Available");
    a.notReached("PreparePackage");
    a.run("HandleStockFailure");
    a.is("CancelOrder", "Available");
    a.run("CancelOrder");
    a.logicalEnd();
    a.is("CancelOrder", "Completed");
}

TEST_CASE("Arkham: out of stock everywhere: PreparePackage must not be left waiting on CheckStock") {
    // PreparePackage depends on CheckStock; the failure path must not leave it dangling.
    Ax a;
    a.data("inStock", "no");
    a.wh()->setStock("AX-NOTEBOOK", 0);
    a.toChecked();
    a.run(Names{"RequestStock", "HandleStockFailure", "CancelOrder"});
    CHECK_FALSE(a.inst()->getProgress().isWaiting("PreparePackage"));
    a.logicalEnd();
}

TEST_CASE("Arkham: warehouse offline raises an error, nothing changes, and recovery works") {
    Ax a;
    a.data("inStock", "no");
    a.toChecked();
    a.wh()->setOnline(false);
    a.assign("RequestStock");
    a.start("RequestStock");
    CHECK_THROWS_AS(a.engine.completeWorkItem(a.id, "RequestStock", "ax-wh"), std::runtime_error);
    a.is("RequestStock", "Started");   // rolled back
    a.wh()->setOnline(true);
    a.done("RequestStock");
    a.is("RequestStock", "Completed");
    a.run("PreparePackage");
    a.run(Names{"SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder"});
    a.logicalEnd();
}

TEST_CASE("Arkham: not delivered goes to ReturnToWarehouse; the order must then be logically closed") {
    Ax a;
    a.until("ReturnToWarehouse");
    a.is("ReturnToWarehouse", "Available");
    a.notReached("CloseOrder");
    a.run("ReturnToWarehouse");
    a.is("ReturnToWarehouse", "Completed");
    a.logicalEnd();
    // Questionable: after the return nothing closes or cancels the order (no CloseOrder/CancelOrder).
    INFO(a.dump());
    CHECK((a.st("CloseOrder") != "Created" || a.st("CancelOrder") != "Created"));
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED before CheckStock opens CancelOrder and ends cleanly") {
    Ax a;
    a.toValidated();   // CheckStock available
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "changed mind"));
    a.is("CheckStock", "Cancelled");
    a.is("CancelOrder", "Available");
    a.run("CancelOrder");
    a.logicalEnd();
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED before validation (ReceiveOrder/ValidateOrder open)") {
    Ax a;
    a.run("ReceiveOrder");   // ValidateOrder available
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "changed mind"));
    INFO(a.dump());
    // The order is cancelled, yet ValidateOrder (not in the cancel list) must not carry on to CheckStock.
    a.is("CancelOrder", "Available");
    a.drain();
    a.logicalEnd();
    a.is("CheckStock", "Created");
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED mid fulfilment (PreparePackage done, delivery steps open)") {
    Ax a;
    a.toPrepared();
    a.run("SelectDeliveryType");   // AssignDriver still open, Dispatch waiting
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "changed mind"));
    a.is("AssignDriver", "Cancelled");
    a.is("CancelOrder", "Available");
    a.run("CancelOrder");
    a.logicalEnd();
    a.notReached("Dispatch");
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED while Dispatch is available") {
    Ax a;
    a.until("Dispatch");
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "changed mind"));
    a.is("Dispatch", "Cancelled");
    a.run("CancelOrder");
    a.logicalEnd();
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED during Deliver (goods already dispatched)") {
    Ax a;
    a.until("Deliver");
    a.assign("Deliver");
    a.start("Deliver");
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "changed mind"));
    INFO(a.dump());
    // Deliver is not in the cancel list, so it keeps running; CancelOrder opens as well.
    // Logical end: both are resolved and the instance completes.
    a.drain();
    a.logicalEnd();
}

TEST_CASE("Arkham: CUSTOMER_CANCELLED after the order is closed does nothing odd") {
    Ax a;
    a.toDispatched();
    a.run(Names{"Deliver", "CloseOrder"});
    a.logicalEnd();
    CHECK_NOTHROW(a.engine.raiseEvent(a.id, "CUSTOMER_CANCELLED", "too late"));
    INFO(a.dump());
    // A closed order must not reopen a CancelOrder.
    a.is("CancelOrder", "Created");
    CHECK(a.inst()->isComplete());
}

TEST_CASE("Arkham: unknown event type is refused or ignored without damage") {
    Ax a;
    a.toValidated();
    try { a.engine.raiseEvent(a.id, "NO_SUCH_EVENT", "x"); } catch (const std::exception&) {}
    a.is("CheckStock", "Available");
}

TEST_CASE("Arkham: rejecting EVERY step individually leaves a logical end") {
    const Names items = axItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        SUBCASE(items[i].c_str()) {
            Ax a;
            a.until(items[i]);
            INFO("rejecting " << items[i] << " from: " << a.dump());
            a.assign(items[i]);
            a.start(items[i]);
            CHECK_NOTHROW(a.engine.rejectWork(a.id, items[i], a.who[items[i]], "no good"));
            a.is(items[i], "Rejected");
            // Not stalled right after the reject, and nothing waiting on the rejected step.
            a.notStalled();
            a.drain();
            a.logicalEnd();
        }
    }
}

TEST_CASE("Arkham: rejecting a step straight after assign (not started) is consistent") {
    const Names items = axItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        SUBCASE(items[i].c_str()) {
            Ax a;
            a.until(items[i]);
            INFO("assigned-reject " << items[i] << " from: " << a.dump());
            a.assign(items[i]);
            CHECK_THROWS(a.engine.rejectWork(a.id, items[i], a.who[items[i]], "no good"));
            a.is(items[i], "Assigned");
            a.drain();
            a.logicalEnd();
        }
    }
}

TEST_CASE("Arkham: DeliveryManager cancelling any open step leaves a logical end") {
    const Names items = axItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        SUBCASE(items[i].c_str()) {
            Ax a;
            a.until(items[i]);
            INFO("cancelling " << items[i] << " from: " << a.dump());
            CHECK_NOTHROW(a.engine.cancelWork(a.id, items[i], "ax-dm"));
            a.is(items[i], "Cancelled");
            a.notStalled();
            a.drain();
            a.logicalEnd();
        }
    }
}

TEST_CASE("Arkham: only DeliveryManager may cancel") {
    Ax a;
    a.toValidated();
    CHECK_THROWS(a.engine.cancelWork(a.id, "CheckStock", "ax-wh"));
    CHECK_THROWS(a.engine.cancelWork(a.id, "CheckStock", "ax-clerk"));
    a.is("CheckStock", "Available");
}

TEST_CASE("Arkham: Deliver deadline expires while available, then the delivery can still finish") {
    Ax a;
    a.until("Deliver");
    CHECK_NOTHROW(a.engine.advanceClock(11));
    INFO(a.dump());
    a.drain();                       // assign/start/complete whatever is open
    a.is("Deliver", "Completed");
    a.logicalEnd();   // drain also closed the order
}

TEST_CASE("Arkham: Deliver started, deadline passes, the driver can still complete") {
    Ax a;
    a.until("Deliver");
    a.assign("Deliver");
    a.start("Deliver");
    CHECK_NOTHROW(a.engine.advanceClock(11));
    // Overdue started work must not become impossible to finish.
    CHECK_NOTHROW(a.engine.completeWorkItem(a.id, "Deliver", "ax-drv"));
    a.is("Deliver", "Completed");
    if (a.st("Deliver") == "Completed") { a.run("CloseOrder"); a.logicalEnd(); }
    else { a.drain(); a.logicalEnd(); }
}

TEST_CASE("Arkham: undo after a reject restores the item and the progress") {
    Ax a;
    a.until("ValidateOrder");
    a.assign("ValidateOrder");
    a.start("ValidateOrder");
    CHECK_NOTHROW(a.engine.rejectWork(a.id, "ValidateOrder", "ax-clerk", "bad"));
    CHECK(a.engine.canUndo(a.id));
    CHECK_NOTHROW(a.engine.undo(a.id));
    a.is("ValidateOrder", "Started");
    CHECK_FALSE(a.inst()->getProgress().isRejected("ValidateOrder"));
    a.done("ValidateOrder");
    a.is("CheckStock", "Available");
    a.run("CheckStock");
    a.run("PreparePackage");
    a.run(Names{"SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder"});
    a.logicalEnd();
}

TEST_CASE("Arkham: undo of a reject on a branching step restores the branch") {
    Ax a;
    a.until("Deliver");
    a.assign("Deliver");
    a.start("Deliver");
    CHECK_NOTHROW(a.engine.rejectWork(a.id, "Deliver", "ax-drv", "nobody home"));
    CHECK_NOTHROW(a.engine.undo(a.id));
    a.is("Deliver", "Started");
    a.is("CloseOrder", "Created");
    a.is("ReturnToWarehouse", "Created");
    a.done("Deliver");
    a.run("CloseOrder");
    a.logicalEnd();
}

TEST_CASE("Arkham: undo walks all the way back to the start") {
    Ax a;
    a.toChecked();
    int guard = 0;
    while (a.engine.canUndo(a.id) && guard++ < 100) CHECK_NOTHROW(a.engine.undo(a.id));
    INFO(a.dump());
    a.is("ReceiveOrder", "Available");
    CHECK(a.inst()->getAvailableWork() == Names{"ReceiveOrder"});
    a.notReached("ValidateOrder");
}

// =============================== DUNE ===============================

TEST_CASE("Dune: happy path 40t reaches TransportToRefinery") {
    Du d;
    d.harvested();
    d.run("TransportToRefinery");
    d.logicalEnd();
    d.is("TransportToRefinery", "Completed");
    d.notReached("AbandonRun");
    d.notReached("Evacuate");
}

TEST_CASE("Dune: 10t is auto approved") {
    Du d("10");
    d.request();
    INFO(d.dump());
    std::vector<const ApprovalRequest*> r = d.inst()->getApprovals();
    REQUIRE(r.size() == 1);
    CHECK(r.back()->getStatus() == APPROVED);
    d.is("ApproveHarvestPermit", "Completed");
    CHECK(d.inst()->getDataFields().at("permitGranted") == "yes");
    d.is("DeployHarvester", "Available");
    d.run(Names{"DeployHarvester", "DeployCarryall", "Harvest", "TransportToRefinery"});
    d.logicalEnd();
}

TEST_CASE("Dune: 150t needs House management (Manager), operations cannot approve") {
    Du d("150");
    d.request();
    std::vector<const ApprovalRequest*> r = d.inst()->getApprovals();
    REQUIRE(r.size() == 1);
    CHECK(r.back()->getStatus() == PENDING);
    const int rid = r.back()->getId();   // copy: a refused approve rolls back and frees every request object
    CHECK_THROWS(d.engine.approve(d.id, rid, "du-surveyor", "nope"));
    d.is("ApproveHarvestPermit", "Started");
    // du-ops holds Manager as well (OperationsManager+Manager), so the top of the chain accepts.
    CHECK_NOTHROW(d.engine.approve(d.id, rid, "du-ops", "House approves"));
    d.is("ApproveHarvestPermit", "Completed");
    d.run(Names{"DeployHarvester", "DeployCarryall", "Harvest", "TransportToRefinery"});
    d.logicalEnd();
}

TEST_CASE("Dune: approval rejected leads to AbandonRun and a logical end") {
    Du d;
    d.request();
    d.decide(false);
    d.is("ApproveHarvestPermit", "Rejected");
    CHECK(d.inst()->getDataFields().at("permitGranted") == "no");
    d.is("AbandonRun", "Available");
    d.notReached("DeployHarvester");
    d.run("AbandonRun");
    d.logicalEnd();
}

TEST_CASE("Dune: unanswered approval escalates after 3 units, then can still be decided") {
    Du d;
    d.request();
    Names changes;
    CHECK_NOTHROW(changes = d.engine.advanceClock(3));
    CHECK_NOTHROW(changes = d.engine.advanceClock(1));
    INFO(d.dump());
    std::vector<const ApprovalRequest*> r = d.inst()->getApprovals();
    REQUIRE(r.size() == 1);
    CHECK(r.back()->getStatus() == PENDING);
    CHECK(r.back()->getHandler()->getName() == "House management");
    CHECK_NOTHROW(d.engine.approve(d.id, r.back()->getId(), "du-ops", "late"));
    d.run(Names{"DeployHarvester", "DeployCarryall", "Harvest", "TransportToRefinery"});
    d.logicalEnd();
}

TEST_CASE("Dune: ops cannot be left with a permit request nobody can decide (after escalation)") {
    Du d;
    d.request();
    d.engine.advanceClock(10);
    d.engine.advanceClock(10);
    INFO(d.dump());
    // however far time runs, the request must still be decidable or have been resolved
    const bool pending = d.decide(true);
    d.drain();
    (void)pending;
    d.logicalEnd();
}

TEST_CASE("Dune: WORM_SIGN while permit is being approved (nothing deployed yet)") {
    Du d;
    d.request();
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "worm"));
    INFO(d.dump());
    // Evacuate opens; the permit must not remain pending forever after the run is aborted.
    d.drain();
    d.logicalEnd();
    d.is("Evacuate", "Completed");
}

TEST_CASE("Dune: WORM_SIGN before deploy (after permit) cancels deploys, opens Evacuate") {
    Du d;
    d.permit();
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "worm"));
    d.is("DeployHarvester", "Cancelled");
    d.is("DeployCarryall", "Cancelled");
    d.is("Evacuate", "Available");
    d.run("Evacuate");
    d.logicalEnd();
    d.notReached("Harvest");
}

TEST_CASE("Dune: WORM_SIGN while one machine is deployed") {
    Du d;
    d.permit();
    d.run("DeployHarvester");
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "worm"));
    d.is("DeployCarryall", "Cancelled");
    d.is("Evacuate", "Available");
    d.run("Evacuate");
    INFO(d.dump());
    // Harvest waits on a Carryall that was cancelled: it must not stay waiting.
    d.logicalEnd();
}

TEST_CASE("Dune: WORM_SIGN during Harvest") {
    Du d;
    d.deployed();
    d.assign("Harvest");
    d.start("Harvest");
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "worm"));
    d.is("Harvest", "Cancelled");
    d.is("Evacuate", "Available");
    d.run("Evacuate");
    d.logicalEnd();
    d.notReached("TransportToRefinery");
}

TEST_CASE("Dune: WORM_SIGN with Harvest done and TransportToRefinery started") {
    Du d;
    d.harvested();
    d.assign("TransportToRefinery");
    d.start("TransportToRefinery");
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "worm"));
    INFO(d.dump());
    // Transport is not interruptible by the rule; the carryall is in the air. Evacuate may open too.
    d.drain();
    d.logicalEnd();
    d.is("TransportToRefinery", "Completed");
}

TEST_CASE("Dune: Evacuate deadline expires while available, evacuation can still finish") {
    Du d;
    d.until("Evacuate");
    CHECK_NOTHROW(d.engine.advanceClock(4));
    d.drain();
    d.is("Evacuate", "Completed");
    d.logicalEnd();
}

TEST_CASE("Dune: Evacuate started, deadline passes, the pilot can still complete") {
    Du d;
    d.until("Evacuate");
    d.assign("Evacuate");
    d.start("Evacuate");
    CHECK_NOTHROW(d.engine.advanceClock(4));
    CHECK_NOTHROW(d.engine.completeWorkItem(d.id, "Evacuate", "du-pilot"));
    d.is("Evacuate", "Completed");
    d.drain();
    d.logicalEnd();
}

TEST_CASE("Dune: WORM_SIGN after the run is finished does nothing odd") {
    Du d;
    d.harvested();
    d.run("TransportToRefinery");
    d.logicalEnd();
    CHECK_NOTHROW(d.engine.raiseEvent(d.id, "WORM_SIGN", "late"));
    INFO(d.dump());
    d.is("Evacuate", "Created");
    CHECK(d.inst()->isComplete());
}

TEST_CASE("Dune: rejecting EVERY step individually leaves a logical end") {
    const Names items = duItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i] == "ApproveHarvestPermit") continue;   // decided through approvals, below
        SUBCASE(items[i].c_str()) {
            Du d;
            d.until(items[i]);
            INFO("rejecting " << items[i] << " from: " << d.dump());
            d.assign(items[i]);
            d.start(items[i]);
            CHECK_NOTHROW(d.engine.rejectWork(d.id, items[i], d.who[items[i]], "no good"));
            d.is(items[i], "Rejected");
            d.notStalled();
            d.drain();
            d.logicalEnd();
        }
    }
}

TEST_CASE("Dune: rejectWork on the approval item (not via rejectApproval) is consistent") {
    Du d;
    d.until("ApproveHarvestPermit");
    d.assign("ApproveHarvestPermit");
    // Rejecting while merely assigned: permitGranted is never written, so routing falls to AbandonRun.
    try { d.engine.rejectWork(d.id, "ApproveHarvestPermit", "du-ops", "no"); } catch (const std::exception&) {}
    d.drain();
    d.logicalEnd();
}

TEST_CASE("Dune: OperationsManager cancelling any open step leaves a logical end") {
    const Names items = duItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        SUBCASE(items[i].c_str()) {
            Du d;
            d.until(items[i]);
            INFO("cancelling " << items[i] << " from: " << d.dump());
            CHECK_NOTHROW(d.engine.cancelWork(d.id, items[i], "du-ops"));
            d.is(items[i], "Cancelled");
            d.notStalled();
            d.drain();
            d.logicalEnd();
        }
    }
}

TEST_CASE("Dune: only OperationsManager may cancel") {
    Du d;
    d.permit();
    CHECK_THROWS(d.engine.cancelWork(d.id, "DeployHarvester", "du-chief"));
    d.is("DeployHarvester", "Available");
}

TEST_CASE("Dune: undo after approval rejection restores the pending request") {
    Du d;
    d.request();
    d.decide(false);
    CHECK_NOTHROW(d.engine.undo(d.id));
    d.is("ApproveHarvestPermit", "Started");
    d.is("AbandonRun", "Created");
    CHECK(d.inst()->getDataFields().count("permitGranted") == 0);
    std::vector<const ApprovalRequest*> r = d.inst()->getApprovals();
    REQUIRE(r.size() == 1);
    CHECK(r.back()->getStatus() == PENDING);   // the desk must follow the undo too
    CHECK_NOTHROW(d.engine.approve(d.id, r.back()->getId(), "du-ops", "second look"));
    d.is("ApproveHarvestPermit", "Completed");
    d.drain();
    d.logicalEnd();
}

TEST_CASE("Dune: undo after a WORM_SIGN restores the pre-event state") {
    Du d;
    d.permit();
    d.engine.raiseEvent(d.id, "WORM_SIGN", "worm");
    CHECK(d.engine.canUndo(d.id));
    CHECK_NOTHROW(d.engine.undo(d.id));
    d.is("DeployHarvester", "Available");
    d.is("Evacuate", "Created");
}

}  // TEST_SUITE
