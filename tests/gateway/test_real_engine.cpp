// The shipped EngineGateway (code/interface/real/RealEngine) against the real engine.
#include "doctest.h"

#include <algorithm>
#include <stdexcept>

#include "RealEngine.h"

using namespace tividy::ui;

namespace {

const std::string DUNE = "DuneSpiceHarvest";
const std::string ARKHAM = "ArkhamExpress";

WorkItemView item(RealEngine& engine, const std::string& instance, const std::string& name) {
    return engine.getWorkItem(instance, name);
}

ActionOutcome act(RealEngine& engine, const std::string& instance, const std::string& workItem, WorkAction action,
                  const std::string& actor, const std::string& assignee = "", const std::string& reason = "") {
    ActionRequest request;
    request.instanceId = instance;
    request.workItemId = workItem;
    request.action = action;
    request.actorId = actor;
    request.assigneeId = assignee;
    request.reason = reason;
    return engine.act(request);
}

void finish(RealEngine& engine, const std::string& instance, const std::string& workItem, const std::string& who) {
    act(engine, instance, workItem, WorkAction::Assign, "du-ops", who);
    act(engine, instance, workItem, WorkAction::Start, who);
    act(engine, instance, workItem, WorkAction::Complete, who);
}

bool has(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

}  // namespace

TEST_SUITE("gateway") {

TEST_CASE("industries, their forms and their people come from the factories") {
    RealEngine engine;
    const std::vector<IndustryInfo> industries = engine.listIndustries();
    REQUIRE(industries.size() == 2);
    CHECK(industries[0].id == ARKHAM);
    CHECK(industries[1].displayName == "Dune: Spice Harvest");
    CHECK(industries[1].definitions == std::vector<std::string>{DUNE});

    const std::vector<DataField> form = engine.startFields(DUNE, DUNE);
    bool sawYield = false;
    for (std::size_t i = 0; i < form.size(); ++i) {
        CHECK(form[i].key != "runId");   // generated, not typed
        if (form[i].key == "estimatedYieldTonnes") {
            sawYield = true;
            CHECK(form[i].numeric);
            CHECK(form[i].defaultValue == "40");
            CHECK(form[i].label == "Estimated yield tonnes");
        }
    }
    CHECK(sawYield);
    CHECK_THROWS_AS(engine.startFields("Nowhere", DUNE), std::invalid_argument);

    CHECK(engine.listParticipants(DUNE).size() == 4);
    CHECK(engine.listParticipants(ARKHAM)[0].id == "ax-clerk");
    CHECK(engine.eventTypes(DUNE) == std::vector<std::string>{"WORM_SIGN"});
}

TEST_CASE("instances get industry ids, typed data overrides the defaults") {
    RealEngine engine;
    CHECK(engine.startInstance(DUNE, DUNE, {{"field", "Arrakeen"}}) == "RUN-1");
    CHECK(engine.startInstance(DUNE, DUNE, {}) == "RUN-2");
    CHECK(engine.startInstance(ARKHAM, ARKHAM, {}) == "AX-1");
    CHECK(engine.listInstances(DUNE).size() == 2);
    CHECK(engine.listInstances(ARKHAM).size() == 1);
    const InstanceView run = engine.getInstance("RUN-1");
    CHECK(run.data.at("field") == "Arrakeen");
    CHECK(run.data.at("runId") == "RUN-1");
    CHECK(run.definitionVersion == "1.0");
    CHECK_FALSE(run.complete);
    CHECK(engine.getInstance("AX-1").data.at("orderId") == "AX-1");
}

TEST_CASE("work item views report state, tracker status, waiting reasons and deadlines") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    CHECK(item(engine, "RUN-1", "SurveyField").state == "Available");
    CHECK(item(engine, "RUN-1", "SurveyField").trackerStatus == "available");
    CHECK(item(engine, "RUN-1", "Harvest").trackerStatus == "");
    CHECK(engine.listWorkItems("RUN-1").size() == 8);
    CHECK(engine.listAvailableWork("RUN-1").size() == 1);

    finish(engine, "RUN-1", "SurveyField", "du-surveyor");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Start, "du-ops");
    engine.approve("RUN-1", "APR-1", "du-ops");
    finish(engine, "RUN-1", "DeployHarvester", "du-chief");
    const WorkItemView harvest = item(engine, "RUN-1", "Harvest");
    CHECK(harvest.trackerStatus == "waiting");
    CHECK(harvest.waitingOn == std::vector<std::string>{"DeployCarryall"});

    engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    CHECK(item(engine, "RUN-1", "Evacuate").dueAt == engine.currentTime() + 3);
    CHECK(item(engine, "RUN-1", "SurveyField").dueAt == -1);
}

TEST_CASE("act reports the state change, the command and the routing") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    const ActionOutcome assigned = act(engine, "RUN-1", "SurveyField", WorkAction::Assign, "du-ops");   // rule picks
    CHECK(assigned.previousState == "Available");
    CHECK(assigned.newState == "Assigned");
    CHECK(assigned.commandDescription == "Sela Orrin assigned 'SurveyField' to Idris Kane");
    CHECK(assigned.routingDescription.empty());

    act(engine, "RUN-1", "SurveyField", WorkAction::Start, "du-surveyor");
    const ActionOutcome done = act(engine, "RUN-1", "SurveyField", WorkAction::Complete, "du-surveyor");
    CHECK(done.newState == "Completed");
    CHECK(done.routingDescription == "Sequence: SurveyField -> ApproveHarvestPermit");
    CHECK(done.madeAvailable == std::vector<std::string>{"ApproveHarvestPermit"});
    CHECK_FALSE(done.instanceComplete);
}

TEST_CASE("a refused action throws a readable message and changes nothing") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    try {
        act(engine, "RUN-1", "SurveyField", WorkAction::Cancel, "du-chief");
        FAIL("cancel by a crew chief should be refused");
    } catch (const std::exception& e) {
        CHECK(std::string(e.what()) == "Access denied: Mara Voss may not cancel 'SurveyField'");
    }
    CHECK(item(engine, "RUN-1", "SurveyField").state == "Available");
    CHECK(engine.nextUndo("RUN-1") == "");
    CHECK_THROWS_AS(act(engine, "RUN-1", "SurveyField", WorkAction::Reject, "du-surveyor"), std::invalid_argument);
    CHECK_THROWS_AS(engine.getInstance("RUN-9"), std::invalid_argument);
}

TEST_CASE("approvals: listed with their handler, decided by id in any spelling") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    finish(engine, "RUN-1", "SurveyField", "du-surveyor");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Start, "du-ops");

    const std::vector<ApprovalView> approvals = engine.listApprovals("RUN-1");
    REQUIRE(approvals.size() == 1);
    CHECK(approvals[0].id == "APR-1");
    CHECK(approvals[0].status == "Pending");
    CHECK(approvals[0].approverRole == "OperationsManager");
    CHECK(approvals[0].workItemId == "ApproveHarvestPermit");

    CHECK_THROWS_AS(engine.approve("RUN-1", "APR-7", "du-ops"), std::invalid_argument);
    CHECK_THROWS_AS(engine.approve("RUN-1", "permit", "du-ops"), std::invalid_argument);
    CHECK_THROWS(engine.approve("RUN-1", "APR-1", "du-pilot"));

    const ActionOutcome outcome = engine.rejectApproval("RUN-1", "apr-1", "du-ops", "storm season");
    CHECK(outcome.newState == "Rejected");
    CHECK(outcome.madeAvailable == std::vector<std::string>{"AbandonRun"});
    CHECK(engine.listApprovals("RUN-1")[0].status == "Rejected");
    CHECK(engine.listApprovals("RUN-1")[0].reason == "storm season");
}

TEST_CASE("events report what changed, and undo reverses them") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    finish(engine, "RUN-1", "SurveyField", "du-surveyor");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Start, "du-ops");
    engine.approve("RUN-1", "APR-1", "du-ops");

    const std::vector<std::string> changes = engine.raiseEvent("RUN-1", "WORM_SIGN", "thumper at 2 km");
    CHECK(has(changes, "DeployHarvester: Available -> Cancelled"));
    CHECK(has(changes, "DeployCarryall: Available -> Cancelled"));
    CHECK(has(changes, "Evacuate: Created -> Available"));
    CHECK_THROWS(engine.raiseEvent("RUN-1", "", ""));

    CHECK(engine.nextUndo("RUN-1") == "event WORM_SIGN");
    CHECK(engine.undo("RUN-1") == "event WORM_SIGN");
    CHECK(item(engine, "RUN-1", "DeployHarvester").state == "Available");
    CHECK(item(engine, "RUN-1", "Evacuate").state == "Created");
}

TEST_CASE("the clock escalates late work, and the reports read the instance") {
    RealEngine engine;
    engine.startInstance(DUNE, DUNE, {});
    finish(engine, "RUN-1", "SurveyField", "du-surveyor");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Assign, "du-ops", "du-ops");
    act(engine, "RUN-1", "ApproveHarvestPermit", WorkAction::Start, "du-ops");
    engine.approve("RUN-1", "APR-1", "du-ops");
    engine.raiseEvent("RUN-1", "WORM_SIGN", "");

    CHECK(engine.currentTime() == 0);
    const std::vector<std::string> changes = engine.advanceClock(5);
    CHECK(engine.currentTime() == 5);
    CHECK(changes == std::vector<std::string>{"RUN-1: 'Evacuate' is overdue and was escalated"});
    CHECK(engine.overdueWork("RUN-1") == std::vector<std::string>{"Evacuate"});
    CHECK(engine.progressSummary("RUN-1").find("8 work items") != std::string::npos);
    CHECK(engine.auditReport("RUN-1")[0] == "-- stage: DuneSpiceHarvest --");
    CHECK_FALSE(engine.history("RUN-1").empty());
}

}  // TEST_SUITE
