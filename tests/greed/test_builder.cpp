#include "doctest.h"
#include "builder/EngineConfigurator.h"
#include "scenarios/arkham/ArkhamExpressBuilder.h"
#include "scenarios/arkham/WarehouseConnector.h"
#include "workflow/Stage.h"
#include "workflow/WorkItem.h"
#include "visitor/WorkflowVisitor.h"
#include "progression/ProgressionTracker.h"
#include "assignment/RoleBasedAssignment.h"
#include <stdexcept>
namespace {
struct Fixture {
    std::shared_ptr<WarehouseStub> api{new WarehouseStub()};
    std::shared_ptr<Connector> adapter{new WarehouseConnector(api)};
    ArkhamExpressBuilder builder{adapter};
    EngineConfigurator director;
    std::unique_ptr<WorkflowDefinition> def;
    Fixture() { api->setStock("AX-NOTEBOOK", 20); def = director.constructScenario(builder); }
};
class Collect : public WorkflowVisitor {
public:
    std::map<std::string, WorkItem*> items;
    int stages = 0;
    void visitWorkItem(WorkItem* item) override { items[item->getName()] = item; }
    void visitStage(Stage*) override { ++stages; }
};
void throughStock(ProgressionTracker& t, const KeyValuePayload& data) {
    t.start(); t.complete("ReceiveOrder", data); t.complete("ValidateOrder", data); t.complete("CheckStock", data);
}
void dispatch(ProgressionTracker& t, const KeyValuePayload& data) {
    t.complete("PreparePackage", data); t.complete("SelectDeliveryType", data);
    CHECK(t.isWaiting("Dispatch"));
    CHECK_THROWS_AS(t.complete("Dispatch", data), std::logic_error);
    t.complete("AssignDriver", data);
    CHECK(t.isAvailable("Dispatch"));
    t.complete("Dispatch", data);
}
// A second tiny builder demonstrates generic reuse, not a claimed completed Industry 2.
class MiniBuilder : public ScenarioBuilder {
public:
    MiniBuilder() : ScenarioBuilder("Mini", "test") {}
    void buildStructure() override { buildStage("Tasks"); buildWorkItem("A", "Clerk", "Tasks"); }
    void buildRules() override { buildEntry("A"); buildAssignmentRule(std::unique_ptr<AssignmentRule>(new RoleBasedAssignment())); }
    void buildConnectors() override {}
    using ScenarioBuilder::buildWorkItem;
    using ScenarioBuilder::buildStage;
    using ScenarioBuilder::buildPath;
    using ScenarioBuilder::buildBranch;
    using ScenarioBuilder::buildDefaultBranch;
    using ScenarioBuilder::buildDependency;
    using ScenarioBuilder::buildConnector;
};
}
TEST_CASE("Director constructs actual definition, stages, items, rules and connector binding") {
    Fixture f; Collect tree; f.def->getRootStage()->accept(&tree);
    CHECK(f.def->getRootStage()->getName() == "ArkhamExpress");
    CHECK(f.def->getVersion() == "1.0");
    CHECK(tree.stages == 5); CHECK(tree.items.size() == 13);
    CHECK(tree.items.at("RequestStock")->getRequiredRole() == "Warehouse");
    CHECK(tree.items.at("Dispatch")->getRequiredRole() == "Driver");
    CHECK(f.def->getEntryItems() == std::vector<std::string>{"ReceiveOrder"});
    REQUIRE(f.def->getConnectorBinding("RequestStock") != nullptr);
    CHECK(f.def->getConnectorBinding("RequestStock")->operation == "requestStock");
    CHECK(f.def->getConnectorBinding("Deliver") == nullptr);
}
TEST_CASE("Builder results own distinct trees and survive builder reset/destruction") {
    Fixture f;
    auto next = f.director.constructScenario(f.builder);
    CHECK(next->getRootStage() != f.def->getRootStage());
    Collect first, second; f.def->getRootStage()->accept(&first); next->getRootStage()->accept(&second);
    CHECK(first.items.at("ReceiveOrder") != second.items.at("ReceiveOrder"));
    CHECK_THROWS_AS(f.builder.getResult(), std::logic_error);
    std::unique_ptr<WorkflowDefinition> surviving;
    { ArkhamExpressBuilder temporary(f.adapter); surviving = f.director.constructScenario(temporary); }
    Collect tree; surviving->getRootStage()->accept(&tree);
    CHECK(tree.items.size() == 13);
}
TEST_CASE("Local stock completes without calling external warehouse and respects dispatch join") {
    Fixture f;
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1");
    throughStock(t, data); dispatch(t, data);
    t.complete("Deliver", data); t.complete("CloseOrder", data);
    CHECK(t.isComplete()); CHECK(f.api->reservationCalls() == 0);
}
TEST_CASE("Invalid order routes to cancellation and never reaches fulfilment") {
    Fixture f;
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1"); data.set("orderValid", "no");
    t.start(); t.complete("ReceiveOrder", data); t.complete("ValidateOrder", data);
    CHECK(t.isAvailable("CancelOrder")); CHECK_FALSE(t.isAvailable("CheckStock"));
    t.complete("CancelOrder", data); CHECK(t.isComplete());
}
TEST_CASE("Adapter response drives Builder's restock success path through existing tracker") {
    Fixture f;
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1"); data.set("inStock", "no");
    throughStock(t, data); REQUIRE(t.isAvailable("RequestStock"));
    const auto* binding = f.def->getConnectorBinding("RequestStock");
    const auto response = binding->connector->invoke(binding->operation, data);
    data = dynamic_cast<const KeyValuePayload&>(*response);
    t.complete("RequestStock", data);
    CHECK(t.isAvailable("PreparePackage"));
    dispatch(t, data); t.complete("Deliver", data); t.complete("CloseOrder", data);
    CHECK(t.isComplete()); CHECK(f.api->remaining("AX-NOTEBOOK") == 18);
}
TEST_CASE("Stock refusal routes to manager handling then cancellation, never packaging") {
    Fixture f; f.api->setStock("AX-NOTEBOOK", 0);
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1"); data.set("inStock", "no");
    throughStock(t, data);
    const auto* binding = f.def->getConnectorBinding("RequestStock");
    auto response = binding->connector->invoke(binding->operation, data);
    data = dynamic_cast<const KeyValuePayload&>(*response);
    t.complete("RequestStock", data);
    CHECK(t.isAvailable("HandleStockFailure")); CHECK_FALSE(t.isAvailable("PreparePackage"));
    t.complete("HandleStockFailure", data); t.complete("CancelOrder", data); CHECK(t.isComplete());
}
TEST_CASE("Transport error leaves request pending for retry when caller invokes before completion") {
    Fixture f; f.api->setOnline(false);
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1"); data.set("inStock", "no");
    throughStock(t, data);
    const auto* binding = f.def->getConnectorBinding("RequestStock");
    CHECK_THROWS_AS(binding->connector->invoke(binding->operation, data), std::runtime_error);
    CHECK(t.isAvailable("RequestStock")); CHECK_FALSE(t.isCompleted("RequestStock"));
    f.api->setOnline(true);
    auto response = binding->connector->invoke(binding->operation, data);
    data = dynamic_cast<const KeyValuePayload&>(*response);
    t.complete("RequestStock", data); CHECK(t.isAvailable("PreparePackage"));
}
TEST_CASE("Delivery rejection routes to return without requiring rejected work to be completed") {
    Fixture f;
    ProgressionTracker t(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto data = ArkhamExpressBuilder::exampleOrder("AX-1"); data.set("delivered", "no");
    throughStock(t, data); dispatch(t, data);
    t.reject("Deliver", data); CHECK(t.isAvailable("ReturnToWarehouse"));
    t.complete("ReturnToWarehouse", data); CHECK(t.isComplete());
}
TEST_CASE("Two trackers share built definition while keeping payload and progression independent") {
    Fixture f;
    ProgressionTracker one(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    ProgressionTracker two(*f.def->getRoutingRule(), f.def->getDependencies(), f.def->getEntryItems());
    auto a = ArkhamExpressBuilder::exampleOrder("AX-1");
    auto b = ArkhamExpressBuilder::exampleOrder("AX-2"); b.set("orderValid", "no");
    throughStock(one, a); two.start();
    CHECK(two.isAvailable("ReceiveOrder"));
    two.complete("ReceiveOrder", b); two.complete("ValidateOrder", b);
    CHECK(two.isAvailable("CancelOrder")); CHECK(one.isAvailable("PreparePackage"));
    CHECK(a.get("orderValid") == "yes");
}
TEST_CASE("Built assignment rule chooses eligible driver; no driver returns null without side effects") {
    Fixture f; Collect tree; f.def->getRootStage()->accept(&tree);
    Participant warehouse("W", "P1"); warehouse.addRole(Role("Warehouse"));
    std::vector<Participant*> pool{&warehouse};
    auto* item = tree.items.at("Dispatch");
    CHECK(f.def->getAssignmentRule()->assignParticipant(item, pool) == nullptr);
    Participant driver("D", "P2"); driver.addRole(Role("Driver")); pool.push_back(&driver);
    CHECK(f.def->getAssignmentRule()->assignParticipant(item, pool) == &driver);
    CHECK(driver.getWorkload() == 0); // selection is not assignment; Commands own accounting.
}
TEST_CASE("Same Director builds another scenario without changing generic code") {
    MiniBuilder builder; EngineConfigurator director;
    auto def = director.constructScenario(builder);
    ProgressionTracker tracker(*def->getRoutingRule(), def->getDependencies(), def->getEntryItems());
    tracker.start(); CHECK(tracker.complete("A", KeyValuePayload()).instanceComplete);
}
TEST_CASE("Builder rejects duplicate names, unknown references and cycles") {
    MiniBuilder b; b.reset(); b.buildStructure(); b.buildRules();
    CHECK_THROWS_AS(b.buildWorkItem("A", "Clerk", "Tasks"), std::invalid_argument);
    CHECK_THROWS_AS(b.buildStage("A"), std::invalid_argument);
    CHECK_THROWS_AS(b.buildStage("Nested", "Missing"), std::invalid_argument);
    CHECK_THROWS_AS(b.buildPath("A", "Missing"), std::invalid_argument);
    b.buildWorkItem("B", "Clerk", "Tasks"); b.buildPath("A", "B");
    b.buildDependency("B", "A");
    CHECK_THROWS_AS(b.buildDependency("A", "B"), std::invalid_argument);
    CHECK_THROWS_AS(b.buildBranch("A", RoutingCondition::equals("x", "y"), {"B"}), std::invalid_argument);
    CHECK_THROWS_AS(b.buildConnector("A", nullptr, "op"), std::invalid_argument);
}
TEST_CASE("Builder refuses incomplete or unreachable work and can be reset after failure") {
    MiniBuilder b; CHECK_THROWS_AS(b.getResult(), std::logic_error);
    b.reset(); CHECK_THROWS_AS(b.getResult(), std::logic_error);
    b.buildStructure(); b.buildRules(); b.buildWorkItem("Lost", "Clerk", "Tasks");
    CHECK_THROWS_AS(b.getResult(), std::logic_error);
    EngineConfigurator director; CHECK_NOTHROW(director.constructScenario(b));
}
TEST_CASE("Stage rejects null/self/duplicate and visits nested stages") {
    Stage root("root");
    CHECK_THROWS_AS(root.add(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(root.add(&root), std::invalid_argument);
    Stage* inner = new Stage("inner"); root.add(inner);
    CHECK_THROWS_AS(root.add(inner), std::invalid_argument);
    WorkItem* leaf = new WorkItem("Leaf", "Clerk"); inner->add(leaf);
    Collect c; root.accept(&c); CHECK(c.stages == 2); CHECK(c.items.size() == 1);
    root.remove(inner); CHECK(root.getChildren().empty());
    CHECK_THROWS_AS(root.accept(nullptr), std::invalid_argument);
    delete leaf; delete inner;   // removed from the tree, so the caller frees them
}
TEST_CASE("Repeated default configuration cannot leave phantom reachable targets") {
    MiniBuilder b; b.reset(); b.buildStructure(); b.buildRules();
    b.buildWorkItem("B", "Clerk", "Tasks"); b.buildWorkItem("C", "Clerk", "Tasks");
    b.buildDefaultBranch("A", {"B"});
    CHECK_THROWS_AS(b.buildDefaultBranch("A", {"C"}), std::invalid_argument);
    CHECK_THROWS_AS(b.getResult(), std::logic_error); // C genuinely remains unreachable.
}
TEST_CASE("Entry dependencies cannot make the whole definition unstartable") {
    MiniBuilder b; b.reset(); b.buildStructure(); b.buildRules();
    b.buildWorkItem("B", "Clerk", "Tasks"); b.buildPath("A", "B");
    b.buildDependency("A", "B");
    CHECK_THROWS_AS(b.getResult(), std::logic_error);
}
