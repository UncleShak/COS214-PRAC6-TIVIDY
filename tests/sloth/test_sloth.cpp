/**
 * @file test_sloth.cpp
 * @brief Unit tests for Sloth components (Observer, History, and Proxy patterns).
 *
 * Uses doctest framework, matching teammates' testing conventions.
 *
 * @author Sloth (The Archivist)
 */
#ifdef STANDALONE_TEST
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#endif
#include "doctest.h"

#include "Event.h"
#include "EventListener.h"
#include "EventSource.h"
#include "NotificationObserver.h"
#include "MonitoringObserver.h"

#include "History.h"
#include "HistoryEntry.h"
#include "InstanceHistory.h"

#include "WorkflowComponent.h"
#include "AccessPolicy.h"
#include "RoleAccessPolicy.h"
#include "AccessControlProxy.h"

#include <stdexcept>
#include <string>
#include <vector>

// ============================================================================
// Support Mocks for Testing
// ============================================================================

class MockComponent : public WorkflowComponent {
public:
    explicit MockComponent(const std::string& name)
        : name(name), executed(false), requestCount(0), childCount(0), visited(false) {}

    std::string getName() const override { return name; }

    void request() override {
        executed = true;
        ++requestCount;
    }

    void add(WorkflowComponent* /*component*/) override {
        ++childCount;
    }

    void remove(WorkflowComponent* /*component*/) override {
        if (childCount > 0) --childCount;
    }

    void accept(WorkflowVisitor* /*visitor*/) override {
        visited = true;
    }

    std::string name;
    bool executed;
    int requestCount;
    int childCount;
    bool visited;
};

class CountingListener : public EventListener {
public:
    CountingListener() : callCount(0) {}
    void onEvent(const Event& event) override {
        ++callCount;
        lastType = event.getType();
    }
    int callCount;
    std::string lastType;
};

// ============================================================================
// 1. Observer Pattern Tests
// ============================================================================

TEST_SUITE("Observer Pattern - Event, EventSource & Listeners") {

    TEST_CASE("Event object encapsulates state change details") {
        Event ev("STATE_CHANGED", "InspectEngine", "Available", "Started", "Assigned to Bob");
        CHECK(ev.getType() == "STATE_CHANGED");
        CHECK(ev.getWorkItemName() == "InspectEngine");
        CHECK(ev.getOldState() == "Available");
        CHECK(ev.getNewState() == "Started");
        CHECK(ev.getDescription() == "Assigned to Bob");
        CHECK(ev.getTimestamp() > 0);
        
        std::string str = ev.toString();
        CHECK(str.find("InspectEngine") != std::string::npos);
        CHECK(str.find("Available -> Started") != std::string::npos);
    }

    TEST_CASE("EventSource attaches, detaches and notifies listeners") {
        EventSource source;
        CHECK(source.getListenerCount() == 0);

        CountingListener listener1;
        CountingListener listener2;

        source.attach(&listener1);
        source.attach(&listener2);
        CHECK(source.getListenerCount() == 2);

        // Dispatches event
        Event ev("TASK_STARTED", "Task1", "Available", "Started");
        source.notifyListeners(ev);

        CHECK(listener1.callCount == 1);
        CHECK(listener1.lastType == "TASK_STARTED");
        CHECK(listener2.callCount == 1);
        CHECK(listener2.lastType == "TASK_STARTED");

        // Detach one listener
        source.detach(&listener1);
        CHECK(source.getListenerCount() == 1);

        Event ev2("TASK_COMPLETED", "Task1", "Started", "Completed");
        source.notifyListeners(ev2);

        CHECK(listener1.callCount == 1); // Unchanged
        CHECK(listener2.callCount == 2); // Notified
        CHECK(listener2.lastType == "TASK_COMPLETED");
    }

    TEST_CASE("NotificationObserver records alert strings on events") {
        NotificationObserver notifier;
        CHECK(notifier.getNotificationCount() == 0);

        Event ev1("TASK_REJECTED", "ReviewDraft", "Started", "Rejected", "Insufficient references");
        notifier.onEvent(ev1);

        CHECK(notifier.getNotificationCount() == 1);
        std::string note = notifier.getNotifications().front();
        CHECK(note.find("[ALERT]") != std::string::npos);
        CHECK(note.find("ReviewDraft") != std::string::npos);
        CHECK(note.find("Rejected") != std::string::npos);

        notifier.clearNotifications();
        CHECK(notifier.getNotificationCount() == 0);
    }

    TEST_CASE("MonitoringObserver aggregates counts and telemetry") {
        MonitoringObserver monitor;
        CHECK(monitor.getTotalEventCount() == 0);

        monitor.onEvent(Event("STATE_CHANGED", "ItemA", "Created", "Available"));
        monitor.onEvent(Event("STATE_CHANGED", "ItemA", "Available", "Started"));
        monitor.onEvent(Event("STATE_CHANGED", "ItemB", "Created", "Available"));
        monitor.onEvent(Event("TASK_COMPLETED", "ItemA", "Started", "Completed"));

        CHECK(monitor.getTotalEventCount() == 4);
        CHECK(monitor.getEventCount("STATE_CHANGED") == 3);
        CHECK(monitor.getEventCount("TASK_COMPLETED") == 1);
        CHECK(monitor.getStateTransitionCount("Available") == 2);
        CHECK(monitor.getStateTransitionCount("Started") == 1);
        CHECK(monitor.getStateTransitionCount("Completed") == 1);

        monitor.reset();
        CHECK(monitor.getTotalEventCount() == 0);
        CHECK(monitor.getEventCount("STATE_CHANGED") == 0);
    }
}

// ============================================================================
// 2. History & Audit System Tests
// ============================================================================

TEST_SUITE("History and Audit System") {

    TEST_CASE("InstanceHistory records string entries directly via History interface") {
        InstanceHistory history("inst-001");
        History* historyInterface = &history; // Polymorphic use (e.g. by AuditDecorator)

        historyInterface->record("started: VerifyDocuments");
        historyInterface->record("completed: VerifyDocuments");

        CHECK(history.size() == 2);
        auto entries = history.getEntries();
        CHECK(entries[0].getDetails() == "started: VerifyDocuments");
        CHECK(entries[1].getDetails() == "completed: VerifyDocuments");
    }

    TEST_CASE("InstanceHistory receives events via EventListener interface") {
        InstanceHistory history("inst-002");
        EventListener* listener = &history;

        Event ev1("STATE_CHANGED", "ApprovalTask", "Created", "Available");
        Event ev2("STATE_CHANGED", "ApprovalTask", "Available", "Assigned", "Assigned to Alice");
        Event ev3("STATE_CHANGED", "ApprovalTask", "Assigned", "Completed");

        listener->onEvent(ev1);
        listener->onEvent(ev2);
        listener->onEvent(ev3);

        CHECK(history.size() == 3);
        CHECK(history.getEntries()[0].getWorkItemName() == "ApprovalTask");
        CHECK(history.getEntries()[0].getNewState() == "Available");
        CHECK(history.getEntries()[1].getNewState() == "Assigned");
        CHECK(history.getEntries()[2].getNewState() == "Completed");
    }

    TEST_CASE("InstanceHistory filters by work item name") {
        InstanceHistory history("inst-003");
        history.onEvent(Event("EVENT", "TaskAlpha", "A", "B"));
        history.onEvent(Event("EVENT", "TaskBeta", "A", "B"));
        history.onEvent(Event("EVENT", "TaskAlpha", "B", "C"));

        auto alphaEntries = history.getEntriesFor("TaskAlpha");
        CHECK(alphaEntries.size() == 2);
        CHECK(alphaEntries[0].getNewState() == "B");
        CHECK(alphaEntries[1].getNewState() == "C");

        auto betaEntries = history.getEntriesFor("TaskBeta");
        CHECK(betaEntries.size() == 1);

        auto noneEntries = history.getEntriesFor("NonExistent");
        CHECK(noneEntries.empty());
    }

    TEST_CASE("InstanceHistory output matches HistoryIterator string list format") {
        InstanceHistory history("inst-004");
        history.onEvent(Event("STATE_CHANGED", "OrderCheck", "Available", "Started"));
        history.record("manual note added");

        std::vector<std::string> strList = history.toStringList();
        CHECK(strList.size() == 2);
        CHECK(strList[0].find("OrderCheck") != std::string::npos);
        CHECK(strList[1].find("manual note added") != std::string::npos);
    }

    TEST_CASE("Strict instance isolation: histories are never shared") {
        InstanceHistory history1("instance-1");
        InstanceHistory history2("instance-2");

        history1.onEvent(Event("EVENT", "ItemX", "Init", "Done"));
        history1.record("Instance 1 private audit log");

        CHECK(history1.size() == 2);
        CHECK(history2.size() == 0);
        CHECK(history2.empty());

        history2.record("Instance 2 private audit log");
        CHECK(history1.size() == 2);
        CHECK(history2.size() == 1);

        // Verification of distinct IDs
        CHECK(history1.getInstanceId() == "instance-1");
        CHECK(history2.getInstanceId() == "instance-2");
    }
}

// ============================================================================
// 3. Proxy Pattern Tests (AccessControlProxy & AccessPolicy)
// ============================================================================

TEST_SUITE("Proxy Pattern - AccessControlProxy & RoleAccessPolicy") {

    TEST_CASE("RoleAccessPolicy correctly evaluates authorization") {
        RoleAccessPolicy policy(true); // admin override enabled

        // Exact match
        CHECK(policy.isAllowed("Doctor", "Doctor", "request"));
        CHECK_FALSE(policy.isAllowed("Nurse", "Doctor", "request"));

        // Empty requiredRole allows any role
        CHECK(policy.isAllowed("Clerk", "", "request"));

        // Admin override
        CHECK(policy.isAllowed("Administrator", "Doctor", "request"));
        CHECK(policy.isAllowed("Admin", "Doctor", "request"));
        CHECK(policy.isAllowed("Manager", "Doctor", "request"));

        // Admin override disabled
        RoleAccessPolicy strictPolicy(false);
        CHECK_FALSE(strictPolicy.isAllowed("Administrator", "Doctor", "request"));
        CHECK(strictPolicy.isAllowed("Doctor", "Doctor", "request"));
    }

    TEST_CASE("AccessControlProxy permits execution when role authorized") {
        MockComponent realItem("TriagePatient");
        RoleAccessPolicy policy;
        AccessControlProxy proxy(&realItem, &policy, "TriageNurse", "TriageNurse");

        CHECK(proxy.getName() == "TriagePatient");
        CHECK_FALSE(realItem.executed);

        // Permitted execution
        proxy.request();

        CHECK(realItem.executed);
        CHECK(realItem.requestCount == 1);
    }

    TEST_CASE("AccessControlProxy blocks execution and throws when unauthorized") {
        MockComponent realItem("ApproveLoan");
        RoleAccessPolicy policy;
        AccessControlProxy proxy(&realItem, &policy, "Teller", "LoanOfficer");

        CHECK_FALSE(realItem.executed);

        // Should throw std::logic_error
        CHECK_THROWS_AS(proxy.request(), std::logic_error);

        // Real subject should not have been executed
        CHECK_FALSE(realItem.executed);
        CHECK(realItem.requestCount == 0);
    }

    TEST_CASE("AccessControlProxy supports dynamic role changes") {
        MockComponent realItem("PrescribeMedication");
        RoleAccessPolicy policy;
        AccessControlProxy proxy(&realItem, &policy, "Intern", "Specialist");

        // Denied as Intern
        CHECK_THROWS_AS(proxy.request(), std::logic_error);
        CHECK_FALSE(realItem.executed);

        // Promoted to Specialist -> allowed
        proxy.setActorRole("Specialist");
        CHECK_NOTHROW(proxy.request());
        CHECK(realItem.executed);
    }

    TEST_CASE("AccessControlProxy transparently delegates composite operations and visitors") {
        MockComponent realItem("ParentStage");
        RoleAccessPolicy policy;
        AccessControlProxy proxy(&realItem, &policy, "Admin", "Admin");

        MockComponent child("ChildTask");
        proxy.add(&child);
        CHECK(realItem.childCount == 1);

        proxy.remove(&child);
        CHECK(realItem.childCount == 0);

        CHECK_FALSE(realItem.visited);
        proxy.accept(nullptr);
        CHECK(realItem.visited);
    }
}
