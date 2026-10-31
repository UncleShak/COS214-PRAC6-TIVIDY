// Integration: the engine runs whole workflows through the Facade.
// Covers Prototype (instance isolation), Command (actions), Adapter (connector at runtime),
// Strategy (assignment), Observer (interrupts and history), Memento (undo) and Visitor.
#include "doctest.h"

#include <memory>
#include <stdexcept>

#include "builder/EngineConfigurator.h"
#include "core/WorkflowEngine.h"
#include "core/WorkflowException.h"
#include "scenarios/arkham/ArkhamExpressBuilder.h"
#include "scenarios/arkham/WarehouseConnector.h"
#include "assignment/RoleBasedAssignment.h"
#include "visitor/ProgressReportVisitor.h"
#include "workflow/WorkItem.h"
#include "participants/Participant.h"

namespace {

std::string stateOf(WorkflowEngine& engine, const std::string& instance, const std::string& item) {
    return engine.getInstance(instance)->getItem(item)->getCurrentState()->getStateName();
}

// Arkham Express deployed with a stocked warehouse and one person per role.
struct Arkham {
    std::shared_ptr<WarehouseStub> warehouse{new WarehouseStub()};
    WorkflowEngine engine;
    Arkham() {
        warehouse->setStock("AX-NOTEBOOK", 20);
        ArkhamExpressBuilder builder(std::shared_ptr<Connector>(new WarehouseConnector(warehouse)));
        engine.deployWorkflow(EngineConfigurator().constructScenario(builder));
        engine.addParticipant("Clara", "clerk", {"Clerk"});
        engine.addParticipant("Wes", "wh", {"Warehouse"});
        engine.addParticipant("Dana", "dm", {"DeliveryManager"});
        engine.addParticipant("Dev", "drv", {"Driver"});
        engine.addParticipant("Boss", "boss", {"Manager"});
    }
    std::string start(const std::string& id, bool inStock = true) {
        KeyValuePayload order = ArkhamExpressBuilder::exampleOrder(id);
        order.set("inStock", inStock ? "yes" : "no");
        return engine.startInstance(id, "ArkhamExpress", &order)->getId();
    }
    // assign + start + complete, by whoever holds the role
    void finish(const std::string& instance, const std::string& item, const std::string& who) {
        engine.assignWork(instance, item, "boss", who);
        engine.startWork(instance, item, who);
        engine.completeWorkItem(instance, item, who);
    }
};

// Tiny workflow with an interrupt: WORM cancels Harvest and opens Evacuate.
class InterruptBuilder : public ScenarioBuilder {
public:
    InterruptBuilder() : ScenarioBuilder("Mini", "1") {}
    void buildStructure() override {
        buildStage("Run");
        buildWorkItem("Survey", "Crew", "Run");
        buildWorkItem("Harvest", "Crew", "Run");
        buildWorkItem("Transport", "Crew", "Run");
        buildWorkItem("Evacuate", "Crew", "Run");
    }
    void buildRules() override {
        buildEntry("Survey");
        buildPath("Survey", "Harvest");
        buildPath("Harvest", "Transport");
        buildInterrupt("WORM", {"Harvest", "Transport"}, {"Evacuate"});
        buildAssignmentRule(std::unique_ptr<AssignmentRule>(new RoleBasedAssignment()));
    }
    void buildConnectors() override {}
};

struct Mini {
    WorkflowEngine engine;
    Mini() {
        InterruptBuilder builder;
        engine.deployWorkflow(EngineConfigurator().constructScenario(builder));
        engine.addParticipant("Paul", "p", {"Crew"});
        engine.startInstance("m1", "Mini", nullptr);
    }
    void finish(const std::string& item) {
        engine.assignWork("m1", item, "p", "p");
        engine.startWork("m1", item, "p");
        engine.completeWorkItem("m1", item, "p");
    }
};

}  // namespace

TEST_SUITE("integration") {

TEST_CASE("starting an instance makes its entry work available") {
    Arkham a; a.start("AX-1");
    CHECK(a.engine.getAvailableWork("AX-1") == std::vector<std::string>{"ReceiveOrder"});
    CHECK(stateOf(a.engine, "AX-1", "ReceiveOrder") == "Available");
    CHECK(stateOf(a.engine, "AX-1", "ValidateOrder") == "Created");
    CHECK(a.engine.listInstances() == std::vector<std::string>{"AX-1"});
}

TEST_CASE("Prototype: two instances never share work items") {
    Arkham a; a.start("AX-1"); a.start("AX-2");
    a.finish("AX-1", "ReceiveOrder", "clerk");
    CHECK(stateOf(a.engine, "AX-1", "ReceiveOrder") == "Completed");
    CHECK(stateOf(a.engine, "AX-2", "ReceiveOrder") == "Available");
    CHECK(a.engine.getInstance("AX-1")->getItem("ReceiveOrder") != a.engine.getInstance("AX-2")->getItem("ReceiveOrder"));
    // The definition's own tree is untouched.
    WorkflowInstance probe("probe", a.engine.getDefinition("ArkhamExpress"), nullptr);
    CHECK(probe.getItem("ReceiveOrder")->getCurrentState()->getStateName() == "Created");
}

TEST_CASE("Arkham Express runs from order to close through the Facade") {
    Arkham a; a.start("AX-1");
    a.finish("AX-1", "ReceiveOrder", "clerk");
    a.finish("AX-1", "ValidateOrder", "clerk");
    a.finish("AX-1", "CheckStock", "wh");
    a.finish("AX-1", "PreparePackage", "wh");
    CHECK(stateOf(a.engine, "AX-1", "SelectDeliveryType") == "Available");
    CHECK(stateOf(a.engine, "AX-1", "AssignDriver") == "Available");
    a.finish("AX-1", "SelectDeliveryType", "dm");
    CHECK(stateOf(a.engine, "AX-1", "Dispatch") == "Created");   // join: waits for AssignDriver
    a.finish("AX-1", "AssignDriver", "dm");
    a.finish("AX-1", "Dispatch", "drv");
    a.finish("AX-1", "Deliver", "drv");
    a.finish("AX-1", "CloseOrder", "clerk");
    CHECK(a.engine.getInstance("AX-1")->isComplete());
    CHECK(stateOf(a.engine, "AX-1", "RequestStock") == "Created");   // branch never taken
    CHECK(a.engine.getParticipant("clerk")->getWorkload() == 0);
}

TEST_CASE("Adapter: completing RequestStock calls the warehouse and routes on its answer") {
    Arkham a; a.start("AX-1", false);
    a.finish("AX-1", "ReceiveOrder", "clerk");
    a.finish("AX-1", "ValidateOrder", "clerk");
    a.finish("AX-1", "CheckStock", "wh");
    CHECK(stateOf(a.engine, "AX-1", "RequestStock") == "Available");
    a.finish("AX-1", "RequestStock", "wh");
    CHECK(a.warehouse->remaining("AX-NOTEBOOK") == 18);
    CHECK(a.engine.getInstance("AX-1")->getDataFields().at("stockReceived") == "yes");
    CHECK(stateOf(a.engine, "AX-1", "PreparePackage") == "Available");
}

TEST_CASE("Adapter: a warehouse outage leaves the work started and changes nothing") {
    Arkham a; a.start("AX-1", false);
    a.finish("AX-1", "ReceiveOrder", "clerk");
    a.finish("AX-1", "ValidateOrder", "clerk");
    a.finish("AX-1", "CheckStock", "wh");
    a.engine.assignWork("AX-1", "RequestStock", "boss", "wh");
    a.engine.startWork("AX-1", "RequestStock", "wh");
    a.warehouse->setOnline(false);
    CHECK_THROWS_AS(a.engine.completeWorkItem("AX-1", "RequestStock", "wh"), std::runtime_error);
    CHECK(stateOf(a.engine, "AX-1", "RequestStock") == "Started");
    a.warehouse->setOnline(true);
    a.engine.completeWorkItem("AX-1", "RequestStock", "wh");
    CHECK(stateOf(a.engine, "AX-1", "PreparePackage") == "Available");
}

TEST_CASE("data-based routing: an invalid order is cancelled") {
    Arkham a; a.start("AX-1");
    a.finish("AX-1", "ReceiveOrder", "clerk");
    a.engine.setData("AX-1", "orderValid", "no");
    a.finish("AX-1", "ValidateOrder", "clerk");
    CHECK(stateOf(a.engine, "AX-1", "CancelOrder") == "Available");
    CHECK(stateOf(a.engine, "AX-1", "CheckStock") == "Created");
}

TEST_CASE("roles and assignee rules are enforced, and refusals change nothing") {
    Arkham a; a.start("AX-1");
    CHECK_THROWS_AS(a.engine.assignWork("AX-1", "ReceiveOrder", "boss", "drv"), AccessDenied);
    a.engine.assignWork("AX-1", "ReceiveOrder", "boss", "clerk");
    CHECK_THROWS_AS(a.engine.startWork("AX-1", "ReceiveOrder", "wh"), std::logic_error);   // not the assignee
    CHECK_THROWS_AS(a.engine.completeWorkItem("AX-1", "ReceiveOrder", "clerk"), std::logic_error);   // not started
    CHECK(stateOf(a.engine, "AX-1", "ReceiveOrder") == "Assigned");
    CHECK_THROWS_AS(a.engine.startWork("AX-1", "NoSuchItem", "clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.engine.startWork("AX-9", "ReceiveOrder", "clerk"), std::invalid_argument);
    CHECK_THROWS_AS(a.engine.startWork("AX-1", "ReceiveOrder", "nobody"), std::invalid_argument);
}

TEST_CASE("Strategy: autoAssign picks the least-loaded person with the role") {
    Arkham a;
    a.engine.addParticipant("Cleo", "clerk2", {"Clerk"});
    a.start("AX-1"); a.start("AX-2");
    const std::string first = a.engine.autoAssign("AX-1", "ReceiveOrder", "boss");
    const std::string second = a.engine.autoAssign("AX-2", "ReceiveOrder", "boss");
    CHECK(first != second);
    CHECK(a.engine.getParticipant(first)->hasRole("Clerk"));
    CHECK(a.engine.getParticipant(second)->hasRole("Clerk"));
}

TEST_CASE("Memento: undo reverses one action at a time, including routing and workload") {
    Arkham a; a.start("AX-1");
    CHECK_FALSE(a.engine.canUndo("AX-1"));
    a.finish("AX-1", "ReceiveOrder", "clerk");
    CHECK(stateOf(a.engine, "AX-1", "ValidateOrder") == "Available");

    CHECK(a.engine.nextUndoLabel("AX-1") == "complete ReceiveOrder");
    CHECK(a.engine.undo("AX-1") == "complete ReceiveOrder");
    CHECK(stateOf(a.engine, "AX-1", "ReceiveOrder") == "Started");
    CHECK(stateOf(a.engine, "AX-1", "ValidateOrder") == "Created");
    CHECK(a.engine.getAvailableWork("AX-1") == std::vector<std::string>{"ReceiveOrder"});
    CHECK(a.engine.getParticipant("clerk")->getWorkload() == 1);

    a.engine.undo("AX-1");   // start
    a.engine.undo("AX-1");   // assign
    CHECK(stateOf(a.engine, "AX-1", "ReceiveOrder") == "Available");
    CHECK(a.engine.getInstance("AX-1")->getItem("ReceiveOrder")->getAssignedParticipant() == nullptr);
    CHECK(a.engine.getParticipant("clerk")->getWorkload() == 0);
    CHECK_FALSE(a.engine.canUndo("AX-1"));
    CHECK_THROWS_AS(a.engine.undo("AX-1"), std::logic_error);

    a.finish("AX-1", "ReceiveOrder", "clerk");   // and it runs forward again
    CHECK(stateOf(a.engine, "AX-1", "ValidateOrder") == "Available");
}

TEST_CASE("Memento: undo restores data too") {
    Arkham a; a.start("AX-1");
    a.engine.setData("AX-1", "orderValid", "no");
    a.engine.undo("AX-1");
    CHECK(a.engine.getInstance("AX-1")->getDataFields().at("orderValid") == "yes");
}

TEST_CASE("Observer: an interrupt event cancels open work and opens the response") {
    Mini m;
    m.finish("Survey");
    m.engine.assignWork("m1", "Harvest", "p", "p");
    m.engine.startWork("m1", "Harvest", "p");
    CHECK(m.engine.getParticipant("p")->getWorkload() == 1);

    m.engine.raiseEvent("m1", "WORM", "worm sign at the north ridge");
    CHECK(stateOf(m.engine, "m1", "Harvest") == "Cancelled");
    CHECK(stateOf(m.engine, "m1", "Transport") == "Created");   // never reached, so nothing to cancel
    CHECK(stateOf(m.engine, "m1", "Evacuate") == "Available");
    CHECK(m.engine.getParticipant("p")->getWorkload() == 0);

    const std::vector<std::string> log = m.engine.getInstance("m1")->getCommandLog();
    CHECK(log.back() == "System cancelled 'Harvest'");

    m.finish("Evacuate");
    CHECK(m.engine.getInstance("m1")->isComplete());
}

TEST_CASE("Observer: unrelated events do nothing, and an interrupt can be undone") {
    Mini m;
    m.finish("Survey");
    m.engine.raiseEvent("m1", "DUST_STORM", "");
    CHECK(stateOf(m.engine, "m1", "Harvest") == "Available");
    m.engine.raiseEvent("m1", "WORM", "");
    CHECK(stateOf(m.engine, "m1", "Harvest") == "Cancelled");
    CHECK(m.engine.undo("m1") == "event WORM");
    CHECK(stateOf(m.engine, "m1", "Harvest") == "Available");
    CHECK(stateOf(m.engine, "m1", "Evacuate") == "Created");
    CHECK(m.engine.getAvailableWork("m1") == std::vector<std::string>{"Harvest"});
}

TEST_CASE("history records commands and state changes in order") {
    Mini m;
    m.finish("Survey");
    const std::vector<std::string> lines = m.engine.getInstance("m1")->getHistory().toStringList();
    REQUIRE(lines.size() >= 4);
    bool sawAssign = false, sawCompleted = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("Paul assigned 'Survey' to Paul") != std::string::npos) sawAssign = true;
        if (lines[i].find("Completed") != std::string::npos) sawCompleted = true;
    }
    CHECK(sawAssign);
    CHECK(sawCompleted);
}

TEST_CASE("Visitor: a progress report runs over an instance's own tree") {
    Arkham a; a.start("AX-1");
    a.finish("AX-1", "ReceiveOrder", "clerk");
    ProgressReportVisitor report;
    a.engine.getInstance("AX-1")->accept(&report);
    CHECK(report.totalItems() == 13);
    CHECK(report.countIn("Completed") == 1);
    CHECK(report.countIn("Available") == 1);
}

TEST_CASE("Facade rejects bad deployments and instance ids") {
    Arkham a;
    CHECK_THROWS_AS(a.engine.startInstance("x", "NoSuchFlow", nullptr), std::invalid_argument);
    a.start("AX-1");
    CHECK_THROWS_AS(a.start("AX-1"), std::invalid_argument);
    CHECK(a.engine.startInstance("", "ArkhamExpress", nullptr)->getId() == "ArkhamExpress-1");
    CHECK_THROWS_AS(a.engine.addParticipant("Again", "clerk", {}), std::invalid_argument);
}

}  // TEST_SUITE
