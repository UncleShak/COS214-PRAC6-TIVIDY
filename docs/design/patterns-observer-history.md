# GoF Pattern Justifications for Sloth

These justifications match the 5-field template owned by Envy (Task 4) and are ready to be integrated into `docs/design/patterns.md` and the final report.

---

## 1. Pattern Justification: Observer (Events, State Changes, and Notification)

**Design problem.**
In a workflow system, whenever work progresses or transitions state (e.g., from `Created` to `Available`, `Assigned`, `Started`, `Completed`, `Rejected`, or `Escalated`), multiple loosely-coupled subsystems need to react immediately without hard-coding dependencies into `WorkItem` or `WorkflowInstance`. For example, audit loggers (`InstanceHistory`) need to capture chronological records, notification handlers (`NotificationObserver`) need to issue alerts or emails to participants, and telemetry monitors (`MonitoringObserver`) need to track performance metrics. If `WorkItem` directly knew about each of these services, it would violate the Open/Closed Principle and single responsibility principle, resulting in tight coupling.

**Participants (C++ classes in Sloth's implementation):**
- **Subject**: `EventSource` declares the interface for attaching, detaching, and notifying observers (`attach()`, `detach()`, `notifyListeners()`).
- **ConcreteSubject**: `WorkItem` (or any engine component generating lifecycle changes) uses or inherits `EventSource` to dispatch notifications.
- **Observer**: `EventListener` defines the abstract updating interface (`onEvent(const Event&)`).
- **ConcreteObserver**:
  - `InstanceHistory`: captures all state transitions in chronological order per workflow instance.
  - `NotificationObserver`: collects alert notifications for critical state transitions.
  - `MonitoringObserver`: computes aggregated statistics and state transition frequencies.
- **Event**: Encapsulates event metadata (type, work item name, old state, new state, description, timestamp).

**Collaboration:**
When a `WorkItem` executes a transition (e.g., in `complete()` or `changeState()`), it creates an `Event` object and dispatches it via `notifyListeners(event)`. The `EventSource` iterates through all registered `EventListener` instances and invokes `onEvent(event)`. Each observer responds according to its own responsibility (e.g. `InstanceHistory` appends an entry to the instance audit log).

**What varies:**
The set of observers that need to react to workflow transitions can vary dynamically and differ per industry without modifying core work item or engine code.

**Why Observer:**
Observer establishes a clean one-to-many dependency between abstractions. It decouples state transition execution from auditing, monitoring, and notification side effects.

---

## 2. Pattern Justification: Proxy (Role-Based Access Control)

**Design problem:**
Workflow activities often require specific authorizations or certifications (e.g., only a qualified "Doctor" may prescribe medicine, or only a "LoanOfficer" may approve a loan). Direct execution of `WorkflowComponent::request()` by unauthorized participants must be refused before any state changes take place. Furthermore, access rules may change across industries or organisations. Hard-coding security checks directly inside `WorkItem` or `Stage` would pollute core business logic with permission-checking code.

**Participants (C++ classes in Sloth's implementation):**
- **Subject**: `WorkflowComponent` provides the common interface for leaves (`WorkItem`), composites (`Stage`), decorators, and proxies (`getName()`, `request()`, `add()`, `remove()`, `accept()`).
- **RealSubject**: `WorkItem` and `Stage` implement the real execution of the work item or composite stage.
- **Proxy**: `AccessControlProxy` implements `WorkflowComponent` and holds a pointer to the `RealSubject`. It intercepts invocations to `request()`, `add()`, and `remove()`.
- **AccessPolicy**: Strategy interface providing `isAllowed(actorRole, requiredRole, action)`.
- **RoleAccessPolicy**: Concrete strategy implementing role-based access control with optional administrative override.

**Collaboration:**
The client (e.g. interface or engine dispatcher) interacts with the `WorkflowComponent` through the `AccessControlProxy`. When `proxy->request()` is called:
1. The proxy delegates authorization verification to `AccessPolicy::isAllowed(actorRole, requiredRole, "request")`.
2. If authorization succeeds, the proxy forwards the invocation directly to `realSubject->request()`.
3. If authorization fails, the proxy intercepts the call, rejects execution, and throws a `std::logic_error` with a descriptive message. The real component is never invoked.

**What varies:**
The authorization rules, participant roles, and access control policies can vary dynamically or per industry without altering `WorkItem` or `Stage`.

**Why Proxy:**
Proxy provides a surrogate to control access to the real subject (Protection Proxy). It is distinct from Decorator: Decorator transparently augments or wraps behavior (such as auditing or retries) and forwards requests, whereas a Protection Proxy acts as an authorization gatekeeper that intentionally blocks unauthorized calls.
