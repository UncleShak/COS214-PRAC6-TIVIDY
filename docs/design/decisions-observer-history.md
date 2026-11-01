# Design Decisions Log for Sloth (The Archivist)

Decisions conforming to the five-field format required by Task 7.

---

## D-S1 · 31 Oct 2026 · Sloth

**Question:** How should workflow state changes and lifecycle events be communicated to external observers (audit loggers, alert handlers, and telemetry monitors) without tightly coupling `WorkItem` to these components?

**Before:** `WorkItem.h` declared `notifyListeners(Event*)` but had no associated `attach()` or `detach()` operations, and no `EventSource` or `EventListener` classes were implemented.

**Change:** Introduced the GoF Observer pattern with `EventSource` (Subject), `EventListener` (Observer), `Event` (notification payload), and concrete observers `NotificationObserver` and `MonitoringObserver`. `Event` encapsulates event type, item name, old state, new state, description, and timestamp.

**Why:** Decouples work item state transitions from the subsystems that react to them (audit logging, email alerts, progress tracking). Adding new observers requires no modification to core engine classes (Open/Closed Principle).

**Diagrams:** Activity Diagram 2 updated to show event dispatch and observer notification; Class diagram updated to include `EventSource`, `EventListener`, and `Event`.

**Code:** `Event.h`, `Event.cpp`, `EventListener.h`, `EventSource.h`, `EventSource.cpp`, `NotificationObserver.h`, `NotificationObserver.cpp`, `MonitoringObserver.h`, `MonitoringObserver.cpp`.

---

## D-S2 · 31 Oct 2026 · Sloth

**Question:** How should execution history be recorded per instance while maintaining compatibility with Gluttony's `AuditDecorator` (which expects a `History*` target) and Envy's `HistoryIterator` (which traverses string entries)?

**Before:** In the Practical 6 design, history recording was conceptually linked to Memento, but no unified history storage class existed. Gluttony introduced `History.h` with `virtual void record(const std::string&) = 0` (D-G7) for `AuditDecorator`, but it was not integrated with state-change events.

**Change:** Implemented `InstanceHistory`, which inherits both Gluttony's `History` interface and Sloth's `EventListener` interface. It stores structured `HistoryEntry` records, provides `toStringList()` for compatibility with `HistoryIterator`, and enforces strict per-instance isolation via unique `instanceId` scoping.

**Why:** Unifies free-form decorator audit entries and structured state-change events in one chronologically ordered log per workflow instance. Guarantees that history from one instance never bleeds into another.

**Diagrams:** Activity Diagram 2 and Class Diagram updated to show `InstanceHistory` realizing `History` and `EventListener`.

**Code:** `History.h`, `HistoryEntry.h`, `HistoryEntry.cpp`, `InstanceHistory.h`, `InstanceHistory.cpp`, `test_sloth.cpp`.

---

## D-S3 · 31 Oct 2026 · Sloth

**Question:** How should the system enforce role-based access control (RBAC) to prevent unauthorized participants from executing work items or stages, without polluting core component logic?

**Before:** In Practical 6, access control was unassigned (listed in Envy's pattern audit as "Not started"). Components had no protection gate on `request()`.

**Change:** Implemented the GoF Protection Proxy pattern via `AccessControlProxy` and an `AccessPolicy` strategy interface. `AccessControlProxy` wraps any `WorkflowComponent` (`WorkItem` or `Stage`) and guards `request()`, `add()`, and `remove()`. `RoleAccessPolicy` verifies participant roles and supports administrative overrides. Unauthorized invocations are rejected with `std::logic_error` before the real subject is touched.

**Why:** Completely separates security/permission concerns from business and workflow logic. Distinct from Decorator (which augments behavior such as retry or timing and always forwards), Proxy acts as an authorization gatekeeper that rejects unauthorized calls.

**Diagrams:** Class Diagram and Activity Diagram 2 updated to show `AccessControlProxy` wrapping `WorkflowComponent` with `AccessPolicy`.

**Code:** `AccessPolicy.h`, `RoleAccessPolicy.h`, `RoleAccessPolicy.cpp`, `AccessControlProxy.h`, `AccessControlProxy.cpp`, `test_sloth.cpp`.
