/**
 * @file test_controller.cpp
 * @brief InterfaceController against FakeEngine: user actions, engine refusals and bad input.
 *
 * Numbers in the test names refer to the test plan in docs/tui-plan.md section 6.2.
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

std::map<std::string, std::string> order(const std::string& amount) {
    std::map<std::string, std::string> values;
    values["amount"] = amount;
    values["destination"] = "Gotham";
    return values;
}

const WorkItemView* item(const InterfaceController& ui, const std::string& name) {
    const std::vector<WorkItemView>& items = ui.view().workItems;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].name == name) {
            return &items[i];
        }
    }
    return 0;
}

std::string stateOf(const InterfaceController& ui, const std::string& name) {
    const WorkItemView* found = item(ui, name);
    return found ? found->state : "<missing>";
}

/** Assign (to whoever has the role), act as the assignee, start and complete. */
StatusMessage finish(InterfaceController& ui, const std::string& name) {
    StatusMessage s = ui.perform(WorkAction::Assign, name);
    if (s.isError()) {
        return s;
    }
    const std::vector<ParticipantView>& people = ui.view().participants;
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (people[i].name == item(ui, name)->assigneeName) {
            ui.setActor(people[i].id);
        }
    }
    s = ui.perform(WorkAction::Start, name);
    if (s.isError()) {
        return s;
    }
    return ui.perform(WorkAction::Complete, name);
}

struct Session {
    FakeEngine engine;
    InterfaceController ui;

    explicit Session(const std::string& amount = "500") : ui(engine) {
        ui.refresh();
        ui.selectIndustry("fake-a");
        ui.startInstance("Fake Order Flow", order(amount));
    }
};

bool starts(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

TEST_CASE("1: industries load and switching industry changes participants and instances") {
    FakeEngine engine;
    InterfaceController ui(engine);
    CHECK_FALSE(ui.refresh().isError());
    REQUIRE(ui.view().industries.size() == 2);

    CHECK_FALSE(ui.selectIndustry("fake-a").isError());
    CHECK(ui.view().participants.front().name == "Alex");
    ui.startInstance("Fake Order Flow", order("10"));
    CHECK(ui.view().instances.size() == 1);

    CHECK_FALSE(ui.selectIndustry("fake-b").isError());
    CHECK(ui.view().participants.front().name == "Eli");
    CHECK(ui.view().instances.empty());
    CHECK(ui.view().instanceId.empty());

    StatusMessage unknown = ui.selectIndustry("nope");
    CHECK(unknown.isError());
    CHECK(starts(unknown.text, "Invalid input:"));
    CHECK(ui.view().industryId == "fake-b");
}

TEST_CASE("2: starting with valid data selects the new instance with its first item Available") {
    Session s;
    CHECK(s.ui.status().kind == StatusMessage::Kind::Success);
    CHECK(s.ui.view().instanceId == "FAKE-0001");
    CHECK(stateOf(s.ui, "Receive") == "Available");
    CHECK(stateOf(s.ui, "Validate") == "Created");
    CHECK(s.ui.view().instance.data.at("urgent") == "no");
}

TEST_CASE("3/4: bad start data is refused before the engine and creates no instance") {
    FakeEngine engine;
    InterfaceController ui(engine);
    ui.refresh();

    CHECK(ui.startInstance("Fake Order Flow", order("10")).isError()); // no industry yet
    ui.selectIndustry("fake-a");

    std::map<std::string, std::string> missing;
    missing["destination"] = "Gotham";
    StatusMessage s = ui.startInstance("Fake Order Flow", missing);
    CHECK(s.text == "Invalid input: Order amount is required");

    s = ui.startInstance("Fake Order Flow", order("a lot"));
    CHECK(s.text == "Invalid input: Order amount must be a number");

    std::map<std::string, std::string> blankDestination = order("10");
    blankDestination["destination"] = "    ";
    CHECK(ui.startInstance("Fake Order Flow", blankDestination).isError());

    CHECK(ui.startInstance("No Such Flow", order("10")).isError());
    CHECK(ui.view().instances.empty());
}

TEST_CASE("6: available work lists only available items, in the order they became available") {
    Session s;
    REQUIRE(s.ui.view().availableWork.size() == 1);
    CHECK(s.ui.view().availableWork[0].name == "Receive");

    finish(s.ui, "Receive");
    REQUIRE(s.ui.view().availableWork.size() == 2);
    CHECK(s.ui.view().availableWork[0].name == "Validate");
    CHECK(s.ui.view().availableWork[1].name == "Check Stock");
}

TEST_CASE("7: assign, start and complete report each state change") {
    Session s;
    StatusMessage m = s.ui.perform(WorkAction::Assign, "Receive");
    CHECK(m.text == "Alex assigned 'Receive' to Alex (Available -> Assigned)");
    CHECK(stateOf(s.ui, "Receive") == "Assigned");
    CHECK(item(s.ui, "Receive")->assigneeName == "Alex");

    m = s.ui.perform(WorkAction::Start, "Receive");
    CHECK(s.ui.lastOutcome().previousState == "Assigned");
    CHECK(s.ui.lastOutcome().newState == "Started");

    m = s.ui.perform(WorkAction::Complete, "Receive");
    CHECK(m.kind == StatusMessage::Kind::Success);
    CHECK(stateOf(s.ui, "Receive") == "Completed");
}

TEST_CASE("8: the decision branch is visible in the routing description") {
    Session low("500");
    finish(low.ui, "Receive");
    CHECK(low.ui.lastOutcome().routingDescription == "Parallel split: Receive -> Validate, Check Stock");
    finish(low.ui, "Validate");
    CHECK(low.ui.lastOutcome().routingDescription == "Decision at Validate: default -> Pack");

    Session high("1500");
    finish(high.ui, "Receive");
    finish(high.ui, "Validate");
    CHECK(high.ui.lastOutcome().routingDescription == "Decision at Validate: amount > 1000 -> Approve");
    CHECK(stateOf(high.ui, "Approve") == "Available");
}

TEST_CASE("9: an item reached before its prerequisite waits, then becomes available") {
    Session s("500");
    finish(s.ui, "Receive");
    finish(s.ui, "Validate"); // routes to Pack, but Pack needs Check Stock
    CHECK(s.ui.lastOutcome().nowWaiting == std::vector<std::string>(1, "Pack"));
    CHECK(stateOf(s.ui, "Pack") == "Created");
    CHECK(item(s.ui, "Pack")->trackerStatus == "waiting");
    CHECK(item(s.ui, "Pack")->waitingOn == std::vector<std::string>(1, "Check Stock"));

    finish(s.ui, "Check Stock");
    CHECK(s.ui.lastOutcome().madeAvailable == std::vector<std::string>(1, "Pack"));
    CHECK(stateOf(s.ui, "Pack") == "Available");
}

TEST_CASE("10: only the assignee may start the work") {
    Session s;
    s.ui.perform(WorkAction::Assign, "Receive", "P-A4"); // Dee
    StatusMessage m = s.ui.perform(WorkAction::Start, "Receive"); // acting as Alex
    CHECK(m.text == "Alex is not assigned to 'Receive'");
    CHECK(stateOf(s.ui, "Receive") == "Assigned");
    CHECK_FALSE(s.ui.hasOutcome());
}

TEST_CASE("11/12: illegal transitions are refused by the engine and change nothing") {
    Session s;
    std::size_t historyBefore = s.engine.history("FAKE-0001").size();

    StatusMessage m = s.ui.perform(WorkAction::Escalate, "Validate"); // still Created
    CHECK(m.text == "cannot escalate 'Validate' while it is Created");
    CHECK(stateOf(s.ui, "Validate") == "Created");

    finish(s.ui, "Receive");
    m = s.ui.perform(WorkAction::Cancel, "Receive"); // Completed is final
    CHECK(m.isError());
    CHECK(stateOf(s.ui, "Receive") == "Completed");

    // Only the successful assign/start/complete were recorded.
    CHECK(s.engine.history("FAKE-0001").size() == historyBefore + 5);
}

TEST_CASE("13: reject needs a reason, and the reason is recorded") {
    Session s;
    s.ui.perform(WorkAction::Assign, "Receive");
    s.ui.perform(WorkAction::Start, "Receive");

    CHECK(s.ui.perform(WorkAction::Reject, "Receive", "", "   ").text ==
          "Invalid input: a reason is required to reject work");
    CHECK(stateOf(s.ui, "Receive") == "Started");

    CHECK_FALSE(s.ui.perform(WorkAction::Reject, "Receive", "", "Address missing").isError());
    CHECK(stateOf(s.ui, "Receive") == "Rejected");

    std::vector<std::string> lines;
    s.ui.history(lines, false, "rejected");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].find("Address missing") != std::string::npos);
}

TEST_CASE("15: routing with no matching branch is an engine error and changes nothing") {
    FakeEngine engine;
    engine.removeDecisionDefault();
    InterfaceController ui(engine);
    ui.refresh();
    ui.selectIndustry("fake-a");
    ui.startInstance("Fake Order Flow", order("10"));
    finish(ui, "Receive");

    StatusMessage m = finish(ui, "Validate");
    CHECK(m.text == "no branch of 'Validate' matches and it has no default");
    CHECK(stateOf(ui, "Validate") == "Started");
}

TEST_CASE("16: unknown IDs and missing selections are reported, not thrown") {
    FakeEngine engine;
    InterfaceController ui(engine);
    ui.refresh();
    CHECK(ui.perform(WorkAction::Start, "Receive").text == "Invalid input: choose an industry first");

    ui.selectIndustry("fake-a");
    CHECK(ui.perform(WorkAction::Start, "Receive").text == "Invalid input: choose or start an instance first");
    CHECK(ui.selectInstance("FAKE-9999").text == "unknown instance 'FAKE-9999'");
    CHECK(ui.view().instanceId.empty());

    ui.startInstance("Fake Order Flow", order("10"));
    CHECK(ui.perform(WorkAction::Assign, "Teleport").text == "unknown work item 'Teleport'");
    CHECK(ui.perform(WorkAction::Assign, "").text == "Invalid input: choose a work item first");
    CHECK(ui.setActor("P-ZZ").isError());
    CHECK(ui.perform(WorkAction::Assign, "Receive", "P-ZZ").isError());
}

TEST_CASE("17/18: history is oldest first, can be reversed and filtered") {
    Session s;
    finish(s.ui, "Receive");

    std::vector<std::string> oldest;
    s.ui.history(oldest, false, "");
    REQUIRE(oldest.size() >= 4);
    CHECK(starts(oldest.front(), "Instance FAKE-0001 started"));

    std::vector<std::string> newest;
    s.ui.history(newest, true, "");
    CHECK(newest.front() == oldest.back());

    std::vector<std::string> filtered;
    s.ui.history(filtered, false, "Routing");
    REQUIRE(filtered.size() == 1);
    CHECK(filtered[0] == "Routing: Parallel split: Receive -> Validate, Check Stock");

    StatusMessage none = s.ui.history(filtered, false, "zebra");
    CHECK(filtered.empty());
    CHECK(none.kind == StatusMessage::Kind::Info);
}

TEST_CASE("19: reports show progress, audit lines and escalated work") {
    Session s;
    finish(s.ui, "Receive");
    s.ui.perform(WorkAction::Escalate, "Check Stock");

    std::string progress;
    std::vector<std::string> audit;
    std::vector<std::string> overdue;
    CHECK_FALSE(s.ui.reports(progress, audit, overdue).isError());
    CHECK(progress == "6 work items: 1 Available, 1 Completed, 3 Created, 1 Escalated (16% complete)");
    CHECK(audit.front() == "Receive | Completed | Alex | role: Clerk");
    CHECK(overdue == std::vector<std::string>(1, "Check Stock"));
}

TEST_CASE("21: the instance completes when the last item is done") {
    Session s("500");
    finish(s.ui, "Receive");
    finish(s.ui, "Check Stock");
    finish(s.ui, "Validate");
    finish(s.ui, "Pack");
    CHECK_FALSE(s.ui.view().instance.complete);
    StatusMessage last = finish(s.ui, "Ship");
    CHECK(s.ui.lastOutcome().instanceComplete);
    CHECK(s.ui.view().instance.complete);
    CHECK(last.text.find("Instance complete.") != std::string::npos);
    CHECK(s.ui.view().availableWork.empty());
}

TEST_CASE("hintedActions mirrors the State pattern's transition table") {
    CHECK(InterfaceController::hintedActions("Created").empty());
    CHECK(InterfaceController::hintedActions("Available").size() == 3);
    CHECK(InterfaceController::hintedActions("Started").front() == WorkAction::Complete);
    CHECK(InterfaceController::hintedActions("Completed").empty());
    CHECK(InterfaceController::hintedActions("Escalated").front() == WorkAction::Assign);
}
