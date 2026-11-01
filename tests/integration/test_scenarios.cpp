// Integration: both industries, installed through their Abstract Factories, run end to end.
#include "doctest.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

#include "core/WorkflowEngine.h"
#include "core/WorkflowException.h"
#include "scenarios/IndustryCatalog.h"
#include "scenarios/arkham/ArkhamExpressFactory.h"
#include "scenarios/dune/DuneHarvestFactory.h"
#include "participants/Participant.h"
#include "workflow/WorkItem.h"
#include "approval/ApprovalRequest.h"
#include "approval/ApprovalHandler.h"

namespace {

struct Dune {
    WorkflowEngine engine;
    DuneHarvestFactory factory;
    Dune() { engine.installIndustry(factory); }
    std::string run(const std::string& id) {
        std::unique_ptr<DataPayload> data = factory.createInitialData(id);
        return engine.startInstance(id, factory.getWorkflowName(), data.get())->getId();
    }
    std::string state(const std::string& id, const std::string& item) {
        return engine.getInstance(id)->getItem(item)->getCurrentState()->getStateName();
    }
    void finish(const std::string& id, const std::string& item, const std::string& who) {
        engine.assignWork(id, item, "du-ops", who);
        engine.startWork(id, item, who);
        engine.completeWorkItem(id, item, who);
    }
    // Survey, then request the permit (40 t: the chain hands it to the operations manager).
    int requestPermit(const std::string& id) {
        finish(id, "SurveyField", "du-surveyor");
        engine.assignWork(id, "ApproveHarvestPermit", "du-ops", "du-ops");
        engine.startWork(id, "ApproveHarvestPermit", "du-ops");
        const std::vector<const ApprovalRequest*> requests = engine.getInstance(id)->getApprovals();
        REQUIRE(requests.size() == 1);
        return requests.back()->getId();
    }
    void throughPermit(const std::string& id) {
        engine.approve(id, requestPermit(id), "du-ops", "field is quiet");
    }
};

bool contains(const std::vector<std::string>& list, const std::string& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

}  // namespace

TEST_SUITE("scenarios") {

TEST_CASE("Abstract Factory: the catalog ships both industries, and both install side by side") {
    std::vector<std::unique_ptr<IndustryFactory> > all = createAllIndustries();
    REQUIRE(all.size() == 2);
    WorkflowEngine engine;
    for (std::size_t i = 0; i < all.size(); ++i) {
        CHECK(engine.installIndustry(*all[i]) == all[i]->getWorkflowName());
        CHECK_FALSE(all[i]->getEventTypes().empty());
        CHECK_FALSE(all[i]->getDisplayName().empty());
    }
    CHECK(engine.listDefinitions() == std::vector<std::string>{"ArkhamExpress", "DuneSpiceHarvest"});
    CHECK(engine.getParticipants().size() == 8);
    CHECK(engine.getParticipant("du-pilot")->hasRole("CarryallPilot"));
    CHECK(engine.getParticipant("ax-drv")->hasRole("Driver"));
    CHECK_THROWS_AS(engine.installIndustry(*all[0]), std::invalid_argument);   // already installed
}

TEST_CASE("Dune: a quiet run goes survey, permit, parallel deployment, harvest, transport") {
    Dune d; d.run("RUN-1");
    CHECK(d.engine.getInstance("RUN-1")->getDataFields().at("field") == "Hagga Basin, sector 7");
    d.throughPermit("RUN-1");
    CHECK(d.state("RUN-1", "DeployHarvester") == "Available");
    CHECK(d.state("RUN-1", "DeployCarryall") == "Available");

    d.finish("RUN-1", "DeployHarvester", "du-chief");
    CHECK(d.state("RUN-1", "Harvest") == "Created");   // waiting for the carryall
    CHECK(contains(d.engine.getInstance("RUN-1")->getProgress().waitingItems(), "Harvest"));
    d.finish("RUN-1", "DeployCarryall", "du-pilot");
    CHECK(d.state("RUN-1", "Harvest") == "Available");

    d.finish("RUN-1", "Harvest", "du-chief");
    d.finish("RUN-1", "TransportToRefinery", "du-pilot");
    CHECK(d.engine.getInstance("RUN-1")->isComplete());
    CHECK(d.state("RUN-1", "Evacuate") == "Created");
    CHECK(d.state("RUN-1", "AbandonRun") == "Created");
}

TEST_CASE("Dune: a denied permit abandons the run") {
    Dune d; d.run("RUN-1");
    d.engine.rejectApproval("RUN-1", d.requestPermit("RUN-1"), "du-ops", "storm season");
    CHECK(d.state("RUN-1", "ApproveHarvestPermit") == "Rejected");
    CHECK(d.engine.getInstance("RUN-1")->getDataFields().at("permitGranted") == "no");
    CHECK(d.state("RUN-1", "AbandonRun") == "Available");
    CHECK(d.state("RUN-1", "DeployHarvester") == "Created");
    d.finish("RUN-1", "AbandonRun", "du-ops");
    CHECK(d.engine.getInstance("RUN-1")->isComplete());
}

TEST_CASE("Dune: worm sign mid-harvest cancels the harvest and opens Evacuate") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.finish("RUN-1", "DeployHarvester", "du-chief");
    d.finish("RUN-1", "DeployCarryall", "du-pilot");
    d.engine.assignWork("RUN-1", "Harvest", "du-ops", "du-chief");
    d.engine.startWork("RUN-1", "Harvest", "du-chief");
    CHECK(d.engine.getParticipant("du-chief")->getWorkload() == 1);

    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "Wormsign! Thumper pattern at 2 km");
    CHECK(d.state("RUN-1", "Harvest") == "Cancelled");
    CHECK(d.state("RUN-1", "Evacuate") == "Available");
    CHECK(d.state("RUN-1", "TransportToRefinery") == "Created");
    CHECK(d.engine.getParticipant("du-chief")->getWorkload() == 0);
    CHECK(d.engine.getInstance("RUN-1")->getCommandLog().back() == "System cancelled 'Harvest'");

    bool logged = false;
    const std::vector<std::string> history = d.engine.getInstance("RUN-1")->getHistory().toStringList();
    for (std::size_t i = 0; i < history.size(); ++i)
        if (history[i].find("Interrupt WORM_SIGN: Wormsign! Thumper pattern at 2 km") != std::string::npos) logged = true;
    CHECK(logged);

    d.finish("RUN-1", "Evacuate", "du-pilot");
    CHECK(d.engine.getInstance("RUN-1")->isComplete());
}

TEST_CASE("Dune: worm sign during deployment stops both machines and the waiting harvest") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.finish("RUN-1", "DeployHarvester", "du-chief");   // carryall still on the pad
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    CHECK(d.state("RUN-1", "DeployHarvester") == "Completed");
    CHECK(d.state("RUN-1", "DeployCarryall") == "Cancelled");
    CHECK(d.state("RUN-1", "Harvest") == "Created");
    CHECK(d.engine.getAvailableWork("RUN-1") == std::vector<std::string>{"Evacuate"});
    CHECK(d.engine.getInstance("RUN-1")->getProgress().waitingItems().empty());
}

TEST_CASE("Dune: worm sign after the harvest still evacuates, and the spice still ships") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.finish("RUN-1", "DeployHarvester", "du-chief");
    d.finish("RUN-1", "DeployCarryall", "du-pilot");
    d.finish("RUN-1", "Harvest", "du-chief");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    CHECK(d.state("RUN-1", "Harvest") == "Completed");
    CHECK(d.state("RUN-1", "TransportToRefinery") == "Available");
    CHECK(d.state("RUN-1", "Evacuate") == "Available");
    d.finish("RUN-1", "Evacuate", "du-pilot");
    d.finish("RUN-1", "TransportToRefinery", "du-pilot");
    CHECK(d.engine.getInstance("RUN-1")->isComplete());
}

TEST_CASE("Dune: undoing the worm sign puts the harvest back exactly as it was") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.finish("RUN-1", "DeployHarvester", "du-chief");
    d.finish("RUN-1", "DeployCarryall", "du-pilot");
    d.engine.assignWork("RUN-1", "Harvest", "du-ops", "du-chief");
    d.engine.startWork("RUN-1", "Harvest", "du-chief");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "false alarm");
    CHECK(d.engine.undo("RUN-1") == "event WORM_SIGN");
    CHECK(d.state("RUN-1", "Harvest") == "Started");
    CHECK(d.state("RUN-1", "Evacuate") == "Created");
    CHECK(d.engine.getInstance("RUN-1")->getItem("Harvest")->getAssignedParticipant() == d.engine.getParticipant("du-chief"));
    CHECK(d.engine.getParticipant("du-chief")->getWorkload() == 1);
    d.engine.completeWorkItem("RUN-1", "Harvest", "du-chief");
    CHECK(d.state("RUN-1", "TransportToRefinery") == "Available");
}

TEST_CASE("Dune: only the operations manager may cancel by hand") {
    Dune d; d.run("RUN-1");
    CHECK_THROWS_AS(d.engine.cancelWork("RUN-1", "SurveyField", "du-chief"), AccessDenied);
    CHECK_THROWS_AS(d.engine.cancelWork("RUN-1", "SurveyField", "du-surveyor"), AccessDenied);
    d.engine.cancelWork("RUN-1", "SurveyField", "du-ops");
    CHECK(d.state("RUN-1", "SurveyField") == "Cancelled");
}

TEST_CASE("Dune: a worm on one field does not touch another run") {
    Dune d; d.run("RUN-1"); d.run("RUN-2");
    d.throughPermit("RUN-1");
    d.throughPermit("RUN-2");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    CHECK(d.state("RUN-1", "DeployHarvester") == "Cancelled");
    CHECK(d.state("RUN-2", "DeployHarvester") == "Available");
    CHECK(d.state("RUN-2", "Evacuate") == "Created");
}

TEST_CASE("Arkham Express: installed from its factory, with its team and example order") {
    WorkflowEngine engine;
    ArkhamExpressFactory factory;
    engine.installIndustry(factory);
    std::unique_ptr<DataPayload> order = factory.createInitialData("AX-7");
    engine.startInstance("AX-7", "ArkhamExpress", order.get());
    CHECK(engine.getInstance("AX-7")->getDataFields().at("orderId") == "AX-7");
    CHECK(engine.autoAssign("AX-7", "ReceiveOrder", "ax-dm") == "ax-clerk");
    CHECK(factory.getWarehouse()->remaining("AX-NOTEBOOK") == 20);
}

TEST_CASE("Arkham Express: the customer cancels mid-fulfilment") {
    WorkflowEngine engine;
    ArkhamExpressFactory factory;
    engine.installIndustry(factory);
    std::unique_ptr<DataPayload> order = factory.createInitialData("AX-7");
    engine.startInstance("AX-7", "ArkhamExpress", order.get());
    const char* const steps[][2] = {{"ReceiveOrder", "ax-clerk"}, {"ValidateOrder", "ax-clerk"}, {"CheckStock", "ax-wh"}};
    for (int i = 0; i < 3; ++i) {
        engine.assignWork("AX-7", steps[i][0], "ax-dm", steps[i][1]);
        engine.startWork("AX-7", steps[i][0], steps[i][1]);
        engine.completeWorkItem("AX-7", steps[i][0], steps[i][1]);
    }
    CHECK_THROWS_AS(engine.cancelWork("AX-7", "PreparePackage", "ax-wh"), AccessDenied);
    engine.raiseEvent("AX-7", "CUSTOMER_CANCELLED", "customer phoned");
    WorkflowInstance* instance = engine.getInstance("AX-7");
    CHECK(instance->getItem("PreparePackage")->getCurrentState()->getStateName() == "Cancelled");
    CHECK(instance->getItem("CancelOrder")->getCurrentState()->getStateName() == "Available");
    engine.assignWork("AX-7", "CancelOrder", "ax-dm", "ax-dm");
    engine.startWork("AX-7", "CancelOrder", "ax-dm");
    engine.completeWorkItem("AX-7", "CancelOrder", "ax-dm");
    CHECK(instance->isComplete());
}

TEST_CASE("Chain of Responsibility: a small yield is approved by the field desk on its own") {
    Dune d; d.run("RUN-1");
    d.engine.setData("RUN-1", "estimatedYieldTonnes", "8");
    d.finish("RUN-1", "SurveyField", "du-surveyor");
    d.engine.assignWork("RUN-1", "ApproveHarvestPermit", "du-ops", "du-ops");
    d.engine.startWork("RUN-1", "ApproveHarvestPermit", "du-ops");
    const std::vector<const ApprovalRequest*> requests = d.engine.getInstance("RUN-1")->getApprovals();
    REQUIRE(requests.size() == 1);
    CHECK(requests[0]->getStatus() == APPROVED);
    CHECK(requests[0]->getDecidedBy() == "auto:Field desk");
    CHECK(d.state("RUN-1", "ApproveHarvestPermit") == "Completed");
    CHECK(d.state("RUN-1", "DeployHarvester") == "Available");
}

TEST_CASE("Chain of Responsibility: only the right role decides, and the permit cannot be completed directly") {
    Dune d; d.run("RUN-1");
    const int id = d.requestPermit("RUN-1");
    CHECK(d.engine.getInstance("RUN-1")->getApprovals()[0]->getHandler()->getName() == "Operations");
    CHECK_THROWS_AS(d.engine.completeWorkItem("RUN-1", "ApproveHarvestPermit", "du-ops"), std::logic_error);
    CHECK_THROWS_AS(d.engine.approve("RUN-1", id, "du-chief"), AccessDenied);
    CHECK_THROWS_AS(d.engine.approve("RUN-1", 99, "du-ops"), std::invalid_argument);
    CHECK_THROWS_AS(d.engine.rejectApproval("RUN-1", id, "du-ops", ""), std::invalid_argument);
    CHECK(d.state("RUN-1", "ApproveHarvestPermit") == "Started");
    d.engine.approve("RUN-1", id, "du-ops");
    CHECK(d.state("RUN-1", "ApproveHarvestPermit") == "Completed");
}

TEST_CASE("Chain of Responsibility: an unanswered permit escalates when the clock passes its limit") {
    Dune d; d.run("RUN-1");
    d.requestPermit("RUN-1");
    CHECK(d.engine.advanceClock(2).empty());
    const std::vector<std::string> changes = d.engine.advanceClock(2);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0] == "RUN-1: APR-1 escalated to House management");
    CHECK(d.engine.getInstance("RUN-1")->getApprovals()[0]->getStatus() == PENDING);
}

TEST_CASE("Decorator: the evacuation deadline escalates late work, and escalates it again once started") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    const int due = d.engine.getInstance("RUN-1")->getDueTime("Evacuate");
    CHECK(due == d.engine.currentTime() + 3);
    const std::vector<std::string> changes = d.engine.advanceClock(4);
    CHECK(changes == std::vector<std::string>{"RUN-1: 'Evacuate' is overdue and was escalated"});
    CHECK(d.state("RUN-1", "Evacuate") == "Escalated");
    CHECK(d.engine.getInstance("RUN-1")->getDueTime("Evacuate") == d.engine.currentTime() + 3);   // fresh deadline

    d.engine.assignWork("RUN-1", "Evacuate", "du-ops", "du-pilot");
    d.engine.startWork("RUN-1", "Evacuate", "du-pilot");
    // Bug 4: late Started work is escalated too, instead of being stuck behind DeadlineExceeded.
    CHECK(d.engine.advanceClock(5) ==
          std::vector<std::string>{"RUN-1: 'Evacuate' is overdue and was escalated (taken from Tobin Reyes)"});
    CHECK(d.state("RUN-1", "Evacuate") == "Escalated");
}

// ---- Bug 4: a started step that misses its deadline can still be finished ----

TEST_CASE("Arkham: Deliver started, deadline passes") {
    WorkflowEngine engine;
    ArkhamExpressFactory factory;
    engine.installIndustry(factory);
    std::unique_ptr<DataPayload> order = factory.createInitialData("AX-7");
    engine.startInstance("AX-7", "ArkhamExpress", order.get());
    const char* const steps[][2] = {{"ReceiveOrder", "ax-clerk"}, {"ValidateOrder", "ax-clerk"},
                                    {"CheckStock", "ax-wh"}, {"PreparePackage", "ax-wh"},
                                    {"SelectDeliveryType", "ax-dm"}, {"AssignDriver", "ax-dm"},
                                    {"Dispatch", "ax-drv"}};
    for (const auto& step : steps) {
        engine.assignWork("AX-7", step[0], "ax-dm", step[1]);
        engine.startWork("AX-7", step[0], step[1]);
        engine.completeWorkItem("AX-7", step[0], step[1]);
    }
    WorkflowInstance* instance = engine.getInstance("AX-7");
    Participant* driver = engine.getParticipant("ax-drv");
    engine.assignWork("AX-7", "Deliver", "ax-dm", "ax-drv");
    engine.startWork("AX-7", "Deliver", "ax-drv");
    CHECK(driver->getWorkload() == 1);

    // The deadline is 10; go past it while Deliver is Started.
    const std::vector<std::string> changes = engine.advanceClock(11);
    CHECK(changes == std::vector<std::string>{"AX-7: 'Deliver' is overdue and was escalated (taken from Rook Calder)"});
    CHECK(instance->getItem("Deliver")->getCurrentState()->getStateName() == "Escalated");
    CHECK(driver->getWorkload() == 0);                                          // taken off the driver
    CHECK(instance->getDueTime("Deliver") == engine.currentTime() + 10);        // fresh deadline

    // Reassigned, it can now be finished, and the order closes normally.
    engine.assignWork("AX-7", "Deliver", "ax-dm", "ax-drv");
    engine.startWork("AX-7", "Deliver", "ax-drv");
    CHECK_NOTHROW(engine.completeWorkItem("AX-7", "Deliver", "ax-drv"));
    CHECK(instance->getItem("Deliver")->getCurrentState()->getStateName() == "Completed");
    CHECK(driver->getWorkload() == 0);
    engine.assignWork("AX-7", "CloseOrder", "ax-dm", "ax-clerk");
    engine.startWork("AX-7", "CloseOrder", "ax-clerk");
    engine.completeWorkItem("AX-7", "CloseOrder", "ax-clerk");
    CHECK(instance->isComplete());
}

TEST_CASE("Dune: Evacuate started, deadline passes") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    d.engine.assignWork("RUN-1", "Evacuate", "du-ops", "du-pilot");
    d.engine.startWork("RUN-1", "Evacuate", "du-pilot");

    d.engine.advanceClock(4);   // deadline is 3
    CHECK(d.state("RUN-1", "Evacuate") == "Escalated");
    CHECK(d.engine.getParticipant("du-pilot")->getWorkload() == 0);

    // Escalated work cannot be cancelled twice off the same person's workload.
    d.engine.assignWork("RUN-1", "Evacuate", "du-ops", "du-pilot");
    d.engine.startWork("RUN-1", "Evacuate", "du-pilot");
    CHECK_NOTHROW(d.engine.completeWorkItem("RUN-1", "Evacuate", "du-pilot"));
    CHECK(d.state("RUN-1", "Evacuate") == "Completed");
    CHECK(d.engine.getParticipant("du-pilot")->getWorkload() == 0);
}

TEST_CASE("Bug 4: work assigned but not started is escalated when overdue, and an escalated step cancels cleanly") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.engine.raiseEvent("RUN-1", "WORM_SIGN", "");
    d.engine.assignWork("RUN-1", "Evacuate", "du-ops", "du-pilot");
    CHECK(d.engine.getParticipant("du-pilot")->getWorkload() == 1);
    d.engine.advanceClock(4);
    CHECK(d.state("RUN-1", "Evacuate") == "Escalated");
    CHECK(d.engine.getParticipant("du-pilot")->getWorkload() == 0);
    d.engine.cancelWork("RUN-1", "Evacuate", "du-ops");
    CHECK(d.state("RUN-1", "Evacuate") == "Cancelled");
    CHECK(d.engine.getParticipant("du-pilot")->getWorkload() == 0);   // not taken off a second time
}

TEST_CASE("Decorator: the harvest is audited into the instance history") {
    Dune d; d.run("RUN-1");
    d.throughPermit("RUN-1");
    d.finish("RUN-1", "DeployHarvester", "du-chief");
    d.finish("RUN-1", "DeployCarryall", "du-pilot");
    d.finish("RUN-1", "Harvest", "du-chief");
    const std::vector<std::string> lines = d.engine.getInstance("RUN-1")->getHistory().toStringList();
    bool started = false, completed = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("started: Harvest") != std::string::npos) started = true;
        if (lines[i].find("completed: Harvest") != std::string::npos) completed = true;
    }
    CHECK(started);
    CHECK(completed);
}

}  // TEST_SUITE
