/**
 * @file test_gateway_real.cpp
 * @brief Drives the shipped RealEngine gateway end to end (both industries) through
 * act()/approve()/raiseEvent()/undo() and checks the views it hands the interface.
 * A failing check documents a gateway or engine bug (nothing under code/ was changed).
 */
#include "doctest.h"

#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "real/RealEngine.h"

using namespace tividy::ui;

namespace {

typedef std::vector<std::string> Names;

ActionRequest req(const std::string& inst, const std::string& item, WorkAction action, const std::string& actor,
                  const std::string& assignee = "", const std::string& reason = "") {
    ActionRequest r;
    r.instanceId = inst; r.workItemId = item; r.action = action; r.actorId = actor;
    r.assigneeId = assignee; r.reason = reason;
    return r;
}

struct Gw {
    RealEngine g;
    // Assign (explicit assignee), start, complete.
    void run(const std::string& inst, const std::string& item, const std::string& manager, const std::string& person) {
        CHECK_NOTHROW(g.act(req(inst, item, WorkAction::Assign, manager, person)));
        CHECK_NOTHROW(g.act(req(inst, item, WorkAction::Start, person)));
        CHECK_NOTHROW(g.act(req(inst, item, WorkAction::Complete, person)));
    }
    WorkItemView item(const std::string& inst, const std::string& name) { return g.getWorkItem(inst, name); }
    std::string ax() { return g.startInstance("ArkhamExpress", "ArkhamExpress", std::map<std::string, std::string>()); }
    std::string du(const std::string& yield = "40") {
        std::map<std::string, std::string> d;
        d["estimatedYieldTonnes"] = yield;
        return g.startInstance("DuneSpiceHarvest", "DuneSpiceHarvest", d);
    }
    void axRun(const std::string& id, const Names& items) {
        std::map<std::string, std::string> who;
        who["ReceiveOrder"] = who["ValidateOrder"] = who["CloseOrder"] = "ax-clerk";
        who["CheckStock"] = who["RequestStock"] = who["PreparePackage"] = "ax-wh";
        who["SelectDeliveryType"] = who["AssignDriver"] = "ax-dm";
        who["Dispatch"] = who["Deliver"] = "ax-drv";
        for (std::size_t i = 0; i < items.size(); ++i) run(id, items[i], "ax-dm", who[items[i]]);
    }
};

bool hasId(const std::vector<WorkItemView>& v, const std::string& name) {
    for (std::size_t i = 0; i < v.size(); ++i) if (v[i].id == name) return true;
    return false;
}

}  // namespace

TEST_CASE("gateway: Arkham end to end through act(), checking every view along the way") {
    Gw w;
    const std::string id = w.ax();
    CHECK(id == "AX-1");
    InstanceView iv = w.g.getInstance(id);
    CHECK_FALSE(iv.complete);
    CHECK_FALSE(iv.stalled);
    CHECK(iv.industryId == "ArkhamExpress");
    CHECK(w.g.listWorkItems(id).size() == 13);

    WorkItemView v = w.item(id, "ReceiveOrder");
    CHECK(v.state == "Available");
    CHECK(v.trackerStatus == "available");
    CHECK(v.requiredRole == "Clerk");
    CHECK(v.assigneeName.empty());
    CHECK(v.dueAt == -1);
    CHECK(w.item(id, "ValidateOrder").state == "Created");
    CHECK(w.item(id, "ValidateOrder").trackerStatus == "");

    ActionOutcome o = w.g.act(req(id, "ReceiveOrder", WorkAction::Assign, "ax-dm"));   // auto-assign
    CHECK(o.previousState == "Available");
    CHECK(o.newState == "Assigned");
    CHECK(o.workItemId == "ReceiveOrder");
    CHECK_FALSE(o.commandDescription.empty());
    CHECK(w.item(id, "ReceiveOrder").assigneeName == "Ada Clerkwell");
    o = w.g.act(req(id, "ReceiveOrder", WorkAction::Start, "ax-clerk"));
    CHECK(o.newState == "Started");
    o = w.g.act(req(id, "ReceiveOrder", WorkAction::Complete, "ax-clerk"));
    CHECK(o.newState == "Completed");
    CHECK(o.madeAvailable == Names(1, "ValidateOrder"));
    CHECK_FALSE(o.routingDescription.empty());
    CHECK(w.item(id, "ReceiveOrder").trackerStatus == "completed");

    w.axRun(id, Names{"ValidateOrder", "CheckStock", "PreparePackage"});
    w.run(id, "SelectDeliveryType", "ax-dm", "ax-dm");
    // Dispatch is reached (waiting) as soon as one of its two prerequisites is done.
    WorkItemView d = w.item(id, "Dispatch");
    CHECK(d.trackerStatus == "waiting");
    CHECK(d.state == "Created");
    CHECK(d.waitingOn == Names(1, "AssignDriver"));
    CHECK_FALSE(w.g.getInstance(id).stalled);
    w.run(id, "AssignDriver", "ax-dm", "ax-dm");
    CHECK(w.item(id, "Dispatch").state == "Available");
    CHECK(w.item(id, "Dispatch").waitingOn.empty());
    w.run(id, "Dispatch", "ax-dm", "ax-drv");
    CHECK(w.item(id, "Deliver").dueAt == w.g.currentTime() + 10);
    CHECK(w.g.listAvailableWork(id).size() == 1);
    CHECK(hasId(w.g.listAvailableWork(id), "Deliver"));
    w.run(id, "Deliver", "ax-dm", "ax-drv");
    o = w.g.act(req(id, "CloseOrder", WorkAction::Assign, "ax-dm", "ax-clerk"));
    w.g.act(req(id, "CloseOrder", WorkAction::Start, "ax-clerk"));
    o = w.g.act(req(id, "CloseOrder", WorkAction::Complete, "ax-clerk"));
    CHECK(o.instanceComplete);
    iv = w.g.getInstance(id);
    CHECK(iv.complete);
    CHECK_FALSE(iv.stalled);
    CHECK(w.g.listAvailableWork(id).empty());
    CHECK(w.g.listInstances("ArkhamExpress").size() == 1);
    CHECK(w.g.listInstances("DuneSpiceHarvest").empty());
    // every participant's workload came back to zero
    const std::vector<ParticipantView> staff = w.g.listParticipants("ArkhamExpress");
    CHECK(staff.size() == 4);
    for (std::size_t i = 0; i < staff.size(); ++i) CHECK(staff[i].workload == 0);
    CHECK(w.g.listApprovals(id).empty());
}

TEST_CASE("gateway: actions on a finished instance are refused and change nothing") {
    Gw w;
    const std::string id = w.ax();
    w.axRun(id, Names{"ReceiveOrder", "ValidateOrder", "CheckStock", "PreparePackage", "SelectDeliveryType",
                      "AssignDriver", "Dispatch", "Deliver", "CloseOrder"});
    REQUIRE(w.g.getInstance(id).complete);
    CHECK_THROWS(w.g.act(req(id, "CloseOrder", WorkAction::Assign, "ax-dm", "ax-clerk")));
    CHECK_THROWS(w.g.act(req(id, "CloseOrder", WorkAction::Start, "ax-clerk")));
    CHECK_THROWS(w.g.act(req(id, "CloseOrder", WorkAction::Complete, "ax-clerk")));
    CHECK_THROWS(w.g.act(req(id, "CloseOrder", WorkAction::Reject, "ax-clerk", "", "why")));
    CHECK_THROWS(w.g.act(req(id, "CloseOrder", WorkAction::Cancel, "ax-dm")));
    CHECK_THROWS(w.g.act(req(id, "ReceiveOrder", WorkAction::Escalate, "ax-clerk")));
    CHECK(w.g.getInstance(id).complete);
    // And a domain event cannot reopen a closed order.
    try { w.g.raiseEvent(id, "CUSTOMER_CANCELLED", "too late"); } catch (const std::exception&) {}
    CHECK(w.g.getInstance(id).complete);
    CHECK(w.item(id, "CancelOrder").state == "Created");
}

TEST_CASE("gateway: bad requests throw sensible errors, never crash") {
    Gw w;
    const std::string id = w.ax();
    CHECK_THROWS_AS(w.g.act(req("NOPE", "ReceiveOrder", WorkAction::Assign, "ax-dm")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "NoSuchItem", WorkAction::Assign, "ax-dm")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "ReceiveOrder", WorkAction::Assign, "ghost")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "ReceiveOrder", WorkAction::Assign, "ax-dm", "ghost")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "", WorkAction::Start, "ax-dm")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "ReceiveOrder", WorkAction::Complete, "")), std::invalid_argument);
    CHECK_THROWS_AS(w.g.act(req(id, "ReceiveOrder", WorkAction::Reject, "ax-clerk", "", "")), std::invalid_argument);
    CHECK_THROWS(w.g.act(req(id, "ReceiveOrder", WorkAction::Start, "ax-clerk")));          // not assigned yet
    CHECK_THROWS(w.g.act(req(id, "ValidateOrder", WorkAction::Assign, "ax-dm")));           // not reached
    CHECK_THROWS(w.g.act(req(id, "ReceiveOrder", WorkAction::Assign, "ax-drv")));           // wrong-role actor
    CHECK_THROWS(w.g.act(req(id, "ReceiveOrder", WorkAction::Assign, "ax-dm", "ax-wh")));   // wrong-role assignee
    CHECK_THROWS(w.g.getWorkItem(id, "NoSuchItem"));
    CHECK_THROWS(w.g.getInstance("NOPE"));
    CHECK_THROWS(w.g.listWorkItems("NOPE"));
    CHECK_THROWS(w.g.listAvailableWork("NOPE"));
    CHECK_THROWS(w.g.history("NOPE"));
    CHECK_THROWS(w.g.progressSummary("NOPE"));
    CHECK_THROWS(w.g.auditReport("NOPE"));
    CHECK_THROWS(w.g.overdueWork("NOPE"));
    CHECK_THROWS(w.g.listApprovals("NOPE"));
    CHECK_THROWS(w.g.nextUndo("NOPE"));
    CHECK_THROWS(w.g.undo("NOPE"));
    CHECK_THROWS(w.g.listInstances("NoIndustry"));
    CHECK_THROWS(w.g.listParticipants("NoIndustry"));
    CHECK_THROWS(w.g.startFields("NoIndustry", "x"));
    CHECK_THROWS(w.g.startFields("ArkhamExpress", "Other"));
    CHECK_THROWS(w.g.startInstance("NoIndustry", "x", std::map<std::string, std::string>()));
    CHECK_THROWS(w.g.startInstance("ArkhamExpress", "Other", std::map<std::string, std::string>()));
    CHECK_THROWS(w.g.eventTypes("NoIndustry"));
    CHECK_THROWS_AS(w.g.advanceClock(0), std::invalid_argument);
    CHECK_THROWS_AS(w.g.advanceClock(-1), std::invalid_argument);
    CHECK(w.g.currentTime() == 0);
    CHECK(w.g.nextUndo(id) == "");
}

TEST_CASE("gateway: startInstance only takes the fields it offered, and ids are unique") {
    Gw w;
    const std::vector<DataField> form = w.g.startFields("ArkhamExpress", "ArkhamExpress");
    std::set<std::string> keys;
    for (std::size_t i = 0; i < form.size(); ++i) keys.insert(form[i].key);
    CHECK(keys.count("orderId") == 0);
    CHECK(keys.count("quantity") == 1);
    for (std::size_t i = 0; i < form.size(); ++i)
        if (form[i].key == "quantity" || form[i].key == "distanceKm") CHECK(form[i].numeric);

    const std::string a = w.ax();
    const std::string b = w.ax();
    CHECK(a != b);
    CHECK(w.g.getInstance(a).data.at("orderId") == a);
    CHECK(w.g.getInstance(b).data.at("orderId") == b);

    // A key the form never offered (here the order's identity) must not be accepted: it would
    // bind order b to a's warehouse reservation.
    std::map<std::string, std::string> sneaky;
    sneaky["orderId"] = a;
    std::string c;
    try { c = w.g.startInstance("ArkhamExpress", "ArkhamExpress", sneaky); } catch (const std::exception&) {}
    if (!c.empty()) CHECK(w.g.getInstance(c).data.at("orderId") == c);
    std::map<std::string, std::string> junk;
    junk["noSuchField"] = "x";
    CHECK_THROWS(w.g.startInstance("ArkhamExpress", "ArkhamExpress", junk));
}

TEST_CASE("gateway: Dune end to end with approvals (ids, status, outcome fields)") {
    Gw w;
    const std::string id = w.du("40");
    CHECK(id == "RUN-1");
    w.run(id, "SurveyField", "du-ops", "du-surveyor");
    CHECK(w.g.listApprovals(id).empty());
    w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
    ActionOutcome o = w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
    CHECK(o.newState == "Started");
    std::vector<ApprovalView> apr = w.g.listApprovals(id);
    REQUIRE(apr.size() == 1);
    CHECK(apr[0].id == "APR-1");
    CHECK(apr[0].workItemId == "ApproveHarvestPermit");
    CHECK(apr[0].status == "Pending");
    CHECK(apr[0].approverRole == "OperationsManager");
    CHECK(apr[0].decidedBy.empty());
    CHECK_FALSE(apr[0].summary.empty());

    // the item is in the approval's hands: completing it directly is refused with a pointer to APR-1
    try { w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Complete, "du-ops")); FAIL("should refuse"); }
    catch (const std::exception& e) { CHECK(std::string(e.what()).find("APR-1") != std::string::npos); }

    CHECK_THROWS(w.g.approve(id, "APR-1", "du-pilot"));                // wrong role
    CHECK_THROWS(w.g.approve(id, "APR-2", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "APR-0", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "APR-", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "APR-x", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "APR-99999999999", "du-ops"));
    CHECK_THROWS(w.g.approve(id, "-1", "du-ops"));
    CHECK_THROWS(w.g.rejectApproval(id, "APR-1", "du-ops", ""));
    CHECK(w.g.listApprovals(id)[0].status == "Pending");

    o = w.g.approve(id, "apr-1", "du-ops");
    CHECK(o.workItemId == "ApproveHarvestPermit");
    CHECK(o.previousState == "Started");
    CHECK(o.newState == "Completed");
    CHECK(o.madeAvailable.size() == 2);
    CHECK_FALSE(o.routingDescription.empty());
    CHECK_FALSE(o.commandDescription.empty());
    CHECK_FALSE(o.instanceComplete);
    apr = w.g.listApprovals(id);
    CHECK(apr[0].status == "Approved");
    CHECK(apr[0].decidedBy == "Sela Orrin");
    CHECK_THROWS(w.g.approve(id, "APR-1", "du-ops"));                  // twice

    w.run(id, "DeployHarvester", "du-ops", "du-chief");
    CHECK(w.item(id, "Harvest").trackerStatus == "waiting");
    CHECK(w.item(id, "Harvest").waitingOn == Names(1, "DeployCarryall"));
    w.run(id, "DeployCarryall", "du-ops", "du-pilot");
    w.run(id, "Harvest", "du-ops", "du-chief");
    w.run(id, "TransportToRefinery", "du-ops", "du-pilot");
    InstanceView iv = w.g.getInstance(id);
    CHECK(iv.complete);
    CHECK_FALSE(iv.stalled);
    CHECK(iv.industryId == "DuneSpiceHarvest");
    CHECK(w.g.listInstances("DuneSpiceHarvest").size() == 1);
    const std::vector<ParticipantView> crew = w.g.listParticipants("DuneSpiceHarvest");
    CHECK(crew.size() == 4);
    for (std::size_t i = 0; i < crew.size(); ++i) CHECK(crew[i].workload == 0);
}

TEST_CASE("gateway: Dune approval levels as the views show them") {
    Gw w;
    SUBCASE("auto approved") {
        const std::string id = w.du("5");
        w.run(id, "SurveyField", "du-ops", "du-surveyor");
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
        const ActionOutcome o = w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
        CHECK(o.newState == "Completed");
        const std::vector<ApprovalView> apr = w.g.listApprovals(id);
        REQUIRE(apr.size() == 1);
        CHECK(apr[0].status == "Approved");
        CHECK(apr[0].decidedBy.find("auto") != std::string::npos);
        CHECK(w.item(id, "DeployHarvester").state == "Available");
    }
    SUBCASE("rejected by the manager sends the run to AbandonRun") {
        const std::string id = w.du("40");
        w.run(id, "SurveyField", "du-ops", "du-surveyor");
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
        const ActionOutcome o = w.g.rejectApproval(id, "APR-1", "du-ops", "worm season");
        CHECK(o.newState == "Rejected");
        CHECK(w.g.listApprovals(id)[0].status == "Rejected");
        CHECK(w.g.listApprovals(id)[0].reason == "worm season");
        CHECK(w.item(id, "AbandonRun").state == "Available");
        CHECK_FALSE(w.g.getInstance(id).stalled);
    }
    SUBCASE("too big for the chain's first levels goes to House management") {
        const std::string id = w.du("500");
        w.run(id, "SurveyField", "du-ops", "du-surveyor");
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
        CHECK(w.g.listApprovals(id)[0].approverRole == "Manager");
    }
    SUBCASE("unanswered request escalates after 3 clock units and the views follow") {
        const std::string id = w.du("40");
        w.run(id, "SurveyField", "du-ops", "du-surveyor");
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
        w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
        CHECK(w.g.advanceClock(2).empty());
        const std::vector<std::string> moved = w.g.advanceClock(1);
        CHECK(moved.size() == 1);
        CHECK(w.g.listApprovals(id)[0].status == "Pending");
        CHECK(w.g.listApprovals(id)[0].approverRole == "Manager");
        CHECK(w.g.currentTime() == 3);
        CHECK_NOTHROW(w.g.approve(id, "APR-1", "du-ops"));
        CHECK(w.item(id, "ApproveHarvestPermit").state == "Completed");
    }
}

TEST_CASE("gateway: WORM_SIGN leaves every view telling the same story") {
    Gw w;
    CHECK(w.g.eventTypes("DuneSpiceHarvest") == Names(1, "WORM_SIGN"));
    CHECK(w.g.eventTypes("ArkhamExpress") == Names(1, "CUSTOMER_CANCELLED"));
    const std::string id = w.du("5");
    w.run(id, "SurveyField", "du-ops", "du-surveyor");
    w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
    w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
    w.run(id, "DeployHarvester", "du-ops", "du-chief");
    CHECK(w.item(id, "Harvest").trackerStatus == "waiting");
    const Names changes = w.g.raiseEvent(id, "WORM_SIGN", "tremors");
    CHECK_FALSE(changes.empty());
    CHECK(w.item(id, "DeployCarryall").state == "Cancelled");
    CHECK(w.item(id, "Evacuate").state == "Available");
    CHECK(w.item(id, "Evacuate").dueAt == w.g.currentTime() + 3);
    // Harvest was waiting: if the tracker says cancelled the item itself must say Cancelled.
    const WorkItemView h = w.item(id, "Harvest");
    INFO("Harvest state=" << h.state << " tracker=" << h.trackerStatus);
    CHECK(((h.trackerStatus == "cancelled") == (h.state == "Cancelled")));
    // overdue work: nothing yet; after the deadline the evacuation is escalated and reported.
    CHECK(w.g.overdueWork(id).empty());
    const Names moved = w.g.advanceClock(4);
    CHECK(moved.size() == 1);
    CHECK(w.g.overdueWork(id) == Names(1, "Evacuate"));
    CHECK(w.item(id, "Evacuate").state == "Escalated");
    CHECK(w.item(id, "Evacuate").trackerStatus == "available");
    // the escalated item gets a fresh deadline and can still be worked to the end
    CHECK(w.item(id, "Evacuate").dueAt == w.g.currentTime() + 3);
    w.g.act(req(id, "Evacuate", WorkAction::Assign, "du-ops", "du-pilot"));
    w.g.act(req(id, "Evacuate", WorkAction::Start, "du-pilot"));
    CHECK_NOTHROW(w.g.act(req(id, "Evacuate", WorkAction::Complete, "du-pilot")));
    CHECK(w.g.overdueWork(id).empty());
    CHECK(w.g.getInstance(id).complete);
    CHECK_FALSE(w.g.getInstance(id).stalled);
    // WORM_SIGN raised on the other industry's instance must not be accepted
    const std::string ax = w.ax();
    CHECK_THROWS(w.g.raiseEvent(ax, "WORM_SIGN", "x"));
    CHECK_THROWS(w.g.raiseEvent(ax, "", "x"));
}

TEST_CASE("gateway: reports and history are consistent and monotonic") {
    Gw w;
    const std::string id = w.ax();
    std::size_t last = w.g.history(id).size();
    CHECK(last > 0);
    const Names steps[] = {Names{"ReceiveOrder"}, Names{"ValidateOrder"}, Names{"CheckStock"}};
    for (int s = 0; s < 3; ++s) {
        w.axRun(id, steps[s]);
        const std::size_t now = w.g.history(id).size();
        CHECK(now > last);
        last = now;
        CHECK_FALSE(w.g.progressSummary(id).empty());
        CHECK(w.g.auditReport(id).size() == 18);               // root + 4 stages + 13 items
    }
    CHECK(w.g.progressSummary(id).find("13 work items") == 0);
    CHECK(w.g.progressSummary(id).find("3 Completed") != std::string::npos);
    // undo keeps the audit trail (never shortens it) but the work state really goes back
    CHECK(w.g.nextUndo(id) == "complete CheckStock");
    CHECK(w.g.undo(id) == "complete CheckStock");
    CHECK(w.g.history(id).size() >= last);
    CHECK(w.item(id, "CheckStock").state == "Started");
    CHECK(w.g.progressSummary(id).find("2 Completed") != std::string::npos);
    CHECK(w.g.nextUndo(id) == "start CheckStock");
    while (w.g.nextUndo(id) != "") w.g.undo(id);
    CHECK(w.g.progressSummary(id).find("Completed") == std::string::npos);
    CHECK(w.item(id, "ReceiveOrder").state == "Available");
    CHECK_THROWS(w.g.undo(id));
    CHECK(w.g.overdueWork(id).empty());
}

TEST_CASE("gateway: undo through the gateway does not leak the clock or other instances") {
    Gw w;
    const std::string a = w.ax();
    const std::string b = w.ax();
    w.axRun(a, Names{"ReceiveOrder"});
    w.g.advanceClock(2);
    CHECK(w.g.nextUndo(a) == "complete ReceiveOrder");   // not a hidden clock entry
    CHECK(w.g.nextUndo(b) == "");
    CHECK(w.g.undo(a) == "complete ReceiveOrder");
    CHECK(w.item(a, "ReceiveOrder").state == "Started");
    CHECK(w.item(b, "ReceiveOrder").state == "Available");
}

TEST_CASE("gateway: failed approve/act calls leave the views untouched") {
    Gw w;
    const std::string id = w.du("40");
    w.run(id, "SurveyField", "du-ops", "du-surveyor");
    w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops"));
    w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Start, "du-ops"));
    const std::vector<WorkItemView> before = w.g.listWorkItems(id);
    CHECK_THROWS(w.g.approve(id, "APR-1", "du-surveyor"));
    CHECK_THROWS(w.g.rejectApproval(id, "APR-1", "du-surveyor", "no"));
    CHECK_THROWS(w.g.act(req(id, "ApproveHarvestPermit", WorkAction::Reject, "du-surveyor", "", "no")));
    const std::vector<WorkItemView> after = w.g.listWorkItems(id);
    REQUIRE(before.size() == after.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        CHECK(before[i].state == after[i].state);
        CHECK(before[i].trackerStatus == after[i].trackerStatus);
    }
    CHECK(w.g.listApprovals(id)[0].status == "Pending");
}

TEST_CASE("gateway: a refused action leaves the audit history untouched") {
    Gw w;
    const std::string id = w.ax();
    const std::vector<std::string> hist = w.g.history(id);
    CHECK_THROWS(w.g.act(req(id, "ReceiveOrder", WorkAction::Start, "ax-clerk")));   // not assigned
    CHECK_THROWS(w.g.act(req(id, "ValidateOrder", WorkAction::Assign, "ax-dm")));    // not reached
    CHECK(w.g.history(id) == hist);   // 'Snapshot restored' lines must not appear for a refusal
}
