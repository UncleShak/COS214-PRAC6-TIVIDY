/**
 * @file test_visitor.cpp
 * @brief Envy: Visitor pattern over the team's work tree (WorkflowComponent, WorkItem, Stage,
 *        TaskDecorator). Uses the test stand-ins in support/ until WorkItem.cpp and Stage exist.
 */
#include "doctest.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "AuditDecorator.h"
#include "AuditReportVisitor.h"
#include "History.h"
#include "OverdueWorkVisitor.h"
#include "Participant.h"
#include "ProgressReportVisitor.h"
#include "WorkItem.h"
#include "WorkItemState.h"
#include "Stage.h"
#include "StandInState.h"

namespace {
std::vector<std::string> list(const char* a) { return std::vector<std::string>(1, a); }
std::vector<std::string> list(const char* a, const char* b) {
    std::vector<std::string> v; v.push_back(a); v.push_back(b); return v;
}

/// Records the order of visits, to check double dispatch.
class RecordingVisitor : public WorkflowVisitor {
public:
    std::vector<std::string> visits;
    void visitWorkItem(WorkItem* item) override { visits.push_back("item:" + item->getName()); }
    void visitStage(Stage* stage) override { visits.push_back("stage:" + stage->getName()); }
};

/// History used by AuditDecorator in these tests.
class ListHistory : public History {
public:
    std::vector<std::string> entries;
    void record(const std::string& entry) override { entries.push_back(entry); }
};

/// ArkhamExpress order (Industry 1), as a work tree:
///   Order { ReceiveOrder, ValidateOrder, Fulfilment { PreparePackage, Dispatch } }
struct OrderTree {
    WorkItem receive, validate, prepare, dispatch;
    Stage order, fulfilment;
    OrderTree()
        : receive("ReceiveOrder", "Clerk"), validate("ValidateOrder", "Clerk"),
          prepare("PreparePackage", "Warehouse"), dispatch("Dispatch", "Driver"),
          order("Order"), fulfilment("Fulfilment") {
        order.add(&receive);
        order.add(&validate);
        order.add(&fulfilment);
        fulfilment.add(&prepare);
        fulfilment.add(&dispatch);
    }
};
}

TEST_SUITE("visitor") {

TEST_CASE("a work item calls visitWorkItem on the visitor (double dispatch)") {
    WorkItem item("ReceiveOrder", "Clerk");
    RecordingVisitor v;
    item.accept(&v);
    CHECK(v.visits == list("item:ReceiveOrder"));
}

TEST_CASE("a stage visits itself, then every child in order, including nested stages") {
    OrderTree t;
    RecordingVisitor v;
    t.order.accept(&v);

    std::vector<std::string> expected;
    expected.push_back("stage:Order");
    expected.push_back("item:ReceiveOrder");
    expected.push_back("item:ValidateOrder");
    expected.push_back("stage:Fulfilment");
    expected.push_back("item:PreparePackage");
    expected.push_back("item:Dispatch");
    CHECK(v.visits == expected);
}

TEST_CASE("decorated work is visited as the real work item (TaskDecorator forwards accept)") {
    WorkItem prepare("PreparePackage", "Warehouse");
    ListHistory history;
    AuditDecorator audited(&prepare, &history);
    Stage stage("Fulfilment");
    stage.add(&audited);

    RecordingVisitor v;
    stage.accept(&v);
    CHECK(v.visits == list("stage:Fulfilment", "item:PreparePackage"));
}

TEST_CASE("progress report counts work by the State pattern's state names") {
    OrderTree t;
    t.receive.changeState(new StandInState("Completed"));
    t.validate.changeState(new StandInState("Completed"));
    t.prepare.changeState(new StandInState("Started"));
    t.dispatch.changeState(new StandInState("Available"));

    ProgressReportVisitor report;
    t.order.accept(&report);

    CHECK(report.totalItems() == 4);
    CHECK(report.totalStages() == 2);
    CHECK(report.countIn("Completed") == 2);
    CHECK(report.countIn("Started") == 1);
    CHECK(report.countIn("Rejected") == 0);
    CHECK(report.percentComplete() == 50);
    CHECK(report.summary() == "4 work items: 1 Available, 2 Completed, 1 Started (50% complete)");
}

TEST_CASE("progress report leaves cancelled work out of the percentage") {
    OrderTree t;
    t.receive.changeState(new StandInState("Completed"));
    t.validate.changeState(new StandInState("Completed"));
    t.prepare.changeState(new StandInState("Completed"));
    t.dispatch.changeState(new StandInState("Cancelled"));

    ProgressReportVisitor report;
    t.order.accept(&report);
    CHECK(report.percentComplete() == 100);
}

TEST_CASE("progress report with nothing to do is 0% and can be reused after reset") {
    ProgressReportVisitor report;
    CHECK(report.percentComplete() == 0);
    CHECK(report.summary() == "0 work items (0% complete)");

    OrderTree t;
    t.order.accept(&report);
    CHECK(report.totalItems() == 4);
    report.reset();
    CHECK(report.totalItems() == 0);
    CHECK(report.countsByState().empty());
}

TEST_CASE("overdue visitor lists escalated work only (nobody acted on it in time)") {
    OrderTree t;
    t.validate.changeState(new StandInState("Escalated"));
    t.dispatch.changeState(new StandInState("Escalated"));
    t.prepare.changeState(new StandInState("Available"));

    OverdueWorkVisitor overdue;
    t.order.accept(&overdue);

    CHECK(overdue.hasOverdueWork());
    CHECK(overdue.overdueNames() == list("ValidateOrder", "Dispatch"));
    CHECK(overdue.overdueItems()[0] == &t.validate);

    overdue.reset();
    CHECK_FALSE(overdue.hasOverdueWork());
}

TEST_CASE("audit report shows each item's state, holder and role in tree order") {
    OrderTree t;
    Participant thandi("Thandi", "P01");
    t.receive.assignTo(&thandi);
    t.receive.changeState(new StandInState("Started"));

    AuditReportVisitor audit;
    t.order.accept(&audit);

    REQUIRE(audit.lines().size() == 6);
    CHECK(audit.lines()[0] == "-- stage: Order --");
    CHECK(audit.lines()[1] == "ReceiveOrder | Started | Thandi | role: Clerk");
    CHECK(audit.lines()[2] == "ValidateOrder | Created | unassigned | role: Clerk");
    CHECK(audit.lines()[5] == "Dispatch | Created | unassigned | role: Driver");
    CHECK(audit.text().find("PreparePackage | Created | unassigned | role: Warehouse\n") != std::string::npos);
}

TEST_CASE("a new report is a new visitor: the work classes do not change") {
    // Counts work per required role, written here without touching WorkItem or Stage.
    class RoleCountVisitor : public WorkflowVisitor {
    public:
        int clerks = 0;
        void visitWorkItem(WorkItem* item) override { if (item->getRequiredRole() == "Clerk") ++clerks; }
        void visitStage(Stage*) override {}
    };
    OrderTree t;
    RoleCountVisitor roles;
    t.order.accept(&roles);
    CHECK(roles.clerks == 2);
}

TEST_CASE("visitors refuse a null work item") {
    ProgressReportVisitor p;
    OverdueWorkVisitor o;
    AuditReportVisitor a;
    CHECK_THROWS_AS(p.visitWorkItem(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(o.visitWorkItem(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(a.visitWorkItem(nullptr), std::invalid_argument);
}

}
