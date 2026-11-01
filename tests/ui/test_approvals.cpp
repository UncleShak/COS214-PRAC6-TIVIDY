/**
 * @file test_approvals.cpp
 * @brief Approvals and the clock through InterfaceController (Wrath's requested commands).
 *
 * FakeEngine's approval rules are a guess at Wrath's ApprovalDesk; update these tests
 * together with the fake when the real API is published.
 *
 * @author Shakir Alexander (Lust)
 */
#include "doctest.h"

#include <map>
#include <string>
#include <vector>

#include "FakeEngine.h"
#include "InterfaceController.h"

using namespace tividy::ui;

namespace {

std::string stateOf(const InterfaceController& ui, const std::string& name) {
    for (std::size_t i = 0; i < ui.view().workItems.size(); ++i) {
        if (ui.view().workItems[i].name == name) {
            return ui.view().workItems[i].state;
        }
    }
    return "<missing>";
}

/** Brings a 1500 order to the point where Approve is waiting for a manager. */
struct BigOrder {
    FakeEngine engine;
    InterfaceController ui;

    BigOrder() : ui(engine) {
        ui.refresh();
        ui.selectIndustry("fake-a");
        std::map<std::string, std::string> values;
        values["amount"] = "1500";
        values["destination"] = "Gotham";
        ui.startInstance("Fake Order Flow", values);
        run("Receive", "P-A1");
        run("Validate", "P-A1");
    }

    void run(const std::string& item, const std::string& person) {
        ui.setActor("P-A1");
        ui.perform(WorkAction::Assign, item, person);
        ui.setActor(person);
        ui.perform(WorkAction::Start, item);
        ui.perform(WorkAction::Complete, item);
    }
};

} // namespace

TEST_CASE("reaching the approval step opens a pending request") {
    BigOrder o;
    REQUIRE(o.ui.view().approvals.size() == 1);
    const ApprovalView& a = o.ui.view().approvals[0];
    CHECK(a.id == "APR-1");
    CHECK(a.status == "Pending");
    CHECK(a.workItemId == "Approve");
    CHECK(a.approverRole == "Manager");
}

TEST_CASE("approve: only the right role may decide, then work routes on") {
    BigOrder o;
    o.ui.setActor("P-A1"); // Alex is a Clerk
    StatusMessage refused = o.ui.approve("APR-1");
    CHECK(refused.text == "Alex cannot decide APR-1: it needs the Manager role");
    CHECK(o.ui.view().approvals[0].status == "Pending");

    o.ui.setActor("P-A2"); // Bo is a Manager
    StatusMessage ok = o.ui.approve("APR-1");
    CHECK(ok.text == "Bo approved APR-1 for 'Approve' (Available -> Completed)");
    CHECK(o.ui.lastOutcome().routingDescription == "Sequence: Approve -> Pack");
    CHECK(o.ui.view().approvals[0].status == "Approved");
    CHECK(o.ui.view().approvals[0].decidedBy == "Bo");

    CHECK(o.ui.approve("APR-1").text == "approval APR-1 is already Approved");
}

TEST_CASE("reject approval: needs a reason, and rejects the waiting work") {
    BigOrder o;
    o.ui.setActor("P-A2");
    CHECK(o.ui.rejectApproval("APR-1", "  ").text == "Invalid input: a reason is required to reject an approval");
    CHECK(o.ui.view().approvals[0].status == "Pending");

    CHECK_FALSE(o.ui.rejectApproval("APR-1", "over budget").isError());
    CHECK(o.ui.view().approvals[0].status == "Rejected");
    CHECK(o.ui.view().approvals[0].reason == "over budget");
    CHECK(stateOf(o.ui, "Approve") == "Rejected");
}

TEST_CASE("approval commands report missing or unknown IDs") {
    BigOrder o;
    CHECK(o.ui.approve("").isError());
    CHECK(o.ui.approve("APR-99").text == "unknown approval 'APR-99'");
}

TEST_CASE("advance: bad units are refused, passing a deadline escalates available work") {
    FakeEngine engine;
    InterfaceController ui(engine);
    ui.refresh();
    ui.selectIndustry("fake-a");
    std::map<std::string, std::string> values;
    values["amount"] = "10";
    values["destination"] = "Gotham";
    ui.startInstance("Fake Order Flow", values);

    std::vector<std::string> changes;
    CHECK(ui.advance("0", changes).isError());
    CHECK(ui.advance("-2", changes).isError());
    CHECK(ui.advance("soon", changes).isError());
    CHECK(ui.view().clock == 0);

    CHECK(ui.advance("5", changes).text == "Clock advanced by 5 to t=5, nothing became overdue");
    CHECK(stateOf(ui, "Receive") == "Available");

    CHECK_FALSE(ui.advance("1", changes).isError());
    REQUIRE(changes.size() == 1);
    CHECK(changes[0] == "FAKE-0001: 'Receive' passed its deadline (t=5) and was escalated");
    CHECK(stateOf(ui, "Receive") == "Escalated");
}
