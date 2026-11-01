// Gluttony's fixes from the other-bugs list: 2 (a returned parcel gives its stock back),
// 7 (only the assignee may reject) and 20 (the clock cannot overflow). Real engine, via the Facade.
#include "doctest.h"

#include <climits>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/WorkflowEngine.h"
#include "decorator/Clock.h"
#include "decorator/DeadlineDecorator.h"
#include "participants/Participant.h"
#include "scenarios/arkham/ArkhamExpressFactory.h"
#include "scenarios/arkham/WarehouseAPI.h"
#include "workflow/WorkItem.h"

namespace {

// Arkham Express installed from its factory, with the factory's warehouse (20 notebooks).
struct Shop {
    WorkflowEngine engine;
    ArkhamExpressFactory factory;
    Shop() { engine.installIndustry(factory); }

    void order(const std::string& id, bool inStock) {
        std::unique_ptr<DataPayload> data = factory.createInitialData(id);
        engine.startInstance(id, "ArkhamExpress", data.get());
        engine.setData(id, "inStock", inStock ? "yes" : "no");
    }
    void finish(const std::string& id, const std::string& item, const std::string& who) {
        engine.assignWork(id, item, "ax-dm", who);
        engine.startWork(id, item, who);
        engine.completeWorkItem(id, item, who);
    }
    std::string state(const std::string& id, const std::string& item) {
        return engine.getInstance(id)->getItem(item)->getCurrentState()->getStateName();
    }
    int stock() const { return factory.getWarehouse()->remaining("AX-NOTEBOOK"); }
};

}  // namespace

TEST_SUITE("gluttony bugs") {

TEST_CASE("Bug 2: a parcel that comes back ends the order as 'returned' and gives its stock back") {
    Shop s;
    s.order("AX-1", false);                      // out of stock, so RequestStock reserves 2
    s.finish("AX-1", "ReceiveOrder", "ax-clerk");
    s.finish("AX-1", "ValidateOrder", "ax-clerk");
    s.finish("AX-1", "CheckStock", "ax-wh");
    s.finish("AX-1", "RequestStock", "ax-wh");
    CHECK(s.stock() == 18);
    s.finish("AX-1", "PreparePackage", "ax-wh");
    s.finish("AX-1", "SelectDeliveryType", "ax-dm");
    s.finish("AX-1", "AssignDriver", "ax-dm");
    s.finish("AX-1", "Dispatch", "ax-drv");
    s.engine.setData("AX-1", "delivered", "no");
    s.finish("AX-1", "Deliver", "ax-drv");
    CHECK(s.state("AX-1", "ReturnToWarehouse") == "Available");

    s.finish("AX-1", "ReturnToWarehouse", "ax-drv");
    CHECK(s.engine.getInstance("AX-1")->isComplete());
    CHECK(s.engine.getInstance("AX-1")->getOutcome() == "returned");
    CHECK(s.stock() == 20);                      // the returned parcel's stock is back

    s.engine.undo("AX-1");                       // undo the return: the stock is taken again
    CHECK(s.stock() == 18);
}

TEST_CASE("Bug 7: only the person doing the work can reject it") {
    Shop s;
    s.engine.addParticipant("Second Stocker", "ax-wh2", {"Warehouse"});
    s.order("AX-1", true);
    s.finish("AX-1", "ReceiveOrder", "ax-clerk");
    s.finish("AX-1", "ValidateOrder", "ax-clerk");
    s.engine.assignWork("AX-1", "CheckStock", "ax-dm", "ax-wh");
    s.engine.startWork("AX-1", "CheckStock", "ax-wh");

    CHECK_THROWS_AS(s.engine.rejectWork("AX-1", "CheckStock", "ax-wh2", "not mine to reject"), std::logic_error);
    CHECK(s.state("AX-1", "CheckStock") == "Started");
    CHECK(s.engine.getParticipant("ax-wh")->getWorkload() == 1);

    s.engine.rejectWork("AX-1", "CheckStock", "ax-wh", "scanner broken");
    CHECK(s.state("AX-1", "CheckStock") == "Rejected");
    CHECK(s.engine.getParticipant("ax-wh")->getWorkload() == 0);
}

TEST_CASE("Bug 20: the clock refuses to overflow and keeps its time") {
    ManualClock clock(INT_MAX - 1);
    clock.advance(1);
    CHECK(clock.now() == INT_MAX);
    CHECK_THROWS_AS(clock.advance(1), std::overflow_error);
    CHECK(clock.now() == INT_MAX);
    CHECK_THROWS_AS(clock.advance(-1), std::invalid_argument);

    WorkflowEngine engine;
    engine.advanceClock(1);
    CHECK_THROWS_AS(engine.advanceClock(INT_MAX), std::overflow_error);
    CHECK(engine.currentTime() == 1);
}

TEST_CASE("Bug 20: a deadline that would pass INT_MAX never arrives instead of wrapping negative") {
    WorkflowEngine engine;
    ArkhamExpressFactory factory;
    engine.installIndustry(factory);
    std::unique_ptr<DataPayload> data = factory.createInitialData("AX-1");
    WorkflowInstance* instance = engine.startInstance("AX-1", "ArkhamExpress", data.get());
    ManualClock clock(INT_MAX - 5);
    DeadlineDecorator late(instance->getItem("ReceiveOrder"), 10, &clock);
    CHECK(late.getDueTime() == INT_MAX);
    CHECK_FALSE(late.isOverdue());
}

}  // TEST_SUITE
