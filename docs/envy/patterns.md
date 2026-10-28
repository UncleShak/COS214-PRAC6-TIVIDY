# TIVIDY GoF patterns: audit, participant mapping and justifications

Owner: Envy (pattern audit). Status as of 26 Oct 2026.

The final project spec (section 4.1) asks for at least 10 distinct, meaningful GoF patterns that **execute** in the system, with at least 2 creational, 2 structural and 2 behavioural, and at least 5 from the listed set (Observer, Iterator, Mediator, Command, Adapter, Chain of Responsibility, Builder, Interpreter, Bridge, Facade, Visitor, Proxy, Singleton, Flyweight). The mappings and pattern class diagrams must describe the actual C++ classes.

This file is checked against the code each time a sin's pull request lands. Every sin sends Envy the justification for any pattern their code implements; Envy updates the row and section below.

## 1. Count check

| Rule | Practical 6 design | Executing in code today | Result |
|---|---|---|---|
| At least 10 distinct patterns | 14 | 1 (Strategy, routing half) | Design passes; code not there yet |
| At least 2 creational | **1** (Builder) | 0 | **Fails. Needs a team decision (section 3)** |
| At least 2 structural | 5 (Composite, Decorator, Adapter, Proxy, Facade) | 0 | Design passes |
| At least 2 behavioural | 8 (State, Strategy, Observer, Chain of Responsibility, Memento, Command, Visitor, Iterator) | 1 | Design passes |
| At least 5 from the listed set | 9 (Observer, Iterator, Command, Adapter, Chain of Responsibility, Builder, Facade, Visitor, Proxy) | 0 | Design passes |

"Executing" means: the class exists in C++, it is called by the running engine, and a test exercises it. A header with pure virtual functions only counts as "interface published".

## 2. Pattern-by-pattern audit

Status key: **Executing** · **Interface published** (header only) · **Not started**.

| # | Pattern | Category | GoF roles → TIVIDY classes | Code owner | Status | Mismatches between code and Practical 6 diagram |
|---|---|---|---|---|---|---|
| 1 | Strategy (routing) | Behavioural | Strategy `RoutingRule`; ConcreteStrategy `SequentialRouting`, `DataBasedRouting`; Context `WorkflowInstance` via `ProgressionTracker` | Envy | **Executing** (29 tests) | Changed on purpose, see D-04 to D-07 and section 4 |
| 1b | Strategy (assignment) | Behavioural | Strategy `AssignmentRule`; ConcreteStrategy `RoleBasedAssignment`, `WorkloadBasedAssignment`; Context `WorkflowInstance` | Greed | Not started | none yet |
| 2 | Composite | Structural | Component `WorkflowComponent`; Leaf `WorkItem`; Composite `Stage` | Pride | Interface published | Diagram's `WorkflowComponent` has `createIterator()`, `proxyRequest()`, `accept()`; header has `getName()`, `add()`, `remove()`, `accept()`. `Stage` has no header yet. Header puts `add`/`remove` on the Component (transparent Composite), so `WorkItem` must implement them, likely by refusing. Record that choice. |
| 3 | State | Behavioural | Context `WorkItem`; State `WorkItemState`; ConcreteState `CreatedState`, `AvailableState`, `AssignedState`, `StartedState`, `CompletedState`, `CancelledState` | Pride | Interface published | Header has one operation per trigger (`makeAvailable`, `assign`, `start`, `complete`, `reject`, `cancel`, `escalate`), but the diagram shows a single `handle()`. The header is the better design, so update the diagram. The header has `reject`/`escalate` but there are no `RejectedState`/`EscalatedState`. Either add them or say which state each one moves to. The diagram has the typo `AssignedSate`. `WorkItem.h` declares `chnageState` (typo for `changeState`). |
| 4 | Observer | Behavioural | Subject `EventSource`; ConcreteSubject `WorkItem`; Observer `EventListener`; ConcreteObserver `WorkflowInstance`, `NotificationObserver`, `MonitoringObserver` | Sloth | Interface published (partly) | `WorkItem.h` has `notifyListeners()` but no `attach`/`detach`, and does not derive from `EventSource`. `EventSource.h` and `EventListener.h` are not written yet. |
| 5 | Memento | Behavioural | Originator `WorkflowInstance`; Memento `InstanceSnapshot`; Caretaker `HistoryManager` | Sloth / Pride | Interface published (partly) | `createSnapshot()`/`restore()` match the diagram. `InstanceSnapshot.h` is missing, so `WorkflowInstance.h` does not compile yet. The snapshot must now also hold the instance's `ProgressionTracker` progress (available, waiting and completed items). Otherwise a rollback restores work-item states but leaves the routing frontier wrong. |
| 6 | Facade | Structural | Facade `WorkflowEngine`; subsystems builder, instances, history, events | Pride | Interface published | Header: `startInstance`, `completeWorkItem`, `assignWork`. Diagram: `deployWorkflow`, `startInstance`, `getAvailableWork`, `completeWorkItem`. `deployWorkflow` and `getAvailableWork` are missing, and Lust's interface needs `getAvailableWork` and a history query. `assignWork` is not on the diagram. |
| 7 | Builder | Creational | Builder `WorkflowBuilder`; ConcreteBuilder `ScenarioBuilder` (one per industry); Director `EngineConfigurator`; Product `WorkflowDefinition` | Greed / Gluttony (industry builders), Pride (product) | Interface published (product only) | Header `WorkflowDefinition` has `getRootStage()`, `getVersion()`; diagram has `getRoot()` and attributes `rootComponent`, `definitionVersion`. Pick one name. The product also needs `getRoutingRule()`, `getDependencies()` and `getEntryItems()` so an instance can build its `ProgressionTracker`. `buildDependency()` should fill a `DependencyGraph`. File name typo: `WorkflowDefnition.h`. |
| 8 | Decorator | Structural | Component `WorkflowComponent`; ConcreteComponent `WorkItem`; Decorator `TaskDecorator`; ConcreteDecorator `ValidationDecorator`, `AuditDecorator`, `DeadlineDecorator`, `RetryDecorator` | Pride (base) | Interface published (`TaskDecorator`) | `TaskDecorator` derives from `WorkflowComponent`, so a decorated `WorkItem` loses the `WorkItem` operations (`changeState`, `assignTo`). Decide whether decorators wrap `WorkItem` instead. The destructor does not delete `wrappedComponent`, so who owns the wrapped object? No concrete decorators yet. |
| 9 | Chain of Responsibility | Behavioural | Handler `ApprovalHandler`; ConcreteHandler `RoleBasedApprover`; Client `WorkItem` | Wrath | Not started | none yet |
| 10 | Command | Behavioural | Command `Command`; ConcreteCommand `AssignCommand`, `StartCommand`, `CompleteCommand`, `RejectCommand`, `CancelCommand`; Receiver `WorkItem`; Invoker `WorkflowEngine` | to agree | Not started | Owner not named in the final split. Lust's interface is the natural invoker. |
| 11 | Adapter | Structural | Target `Connector`; Adapter `ExternalSystemAdapter`; Adaptee `ExternalSystem` (stubbed) | Greed / Gluttony (industry stubs) | Not started | none yet |
| 12 | Visitor | Behavioural | Visitor `WorkflowVisitor`; ConcreteVisitor `ProgressReportVisitor`, `OverdueWorkVisitor`, `AuditReportVisitor`; Element `WorkflowComponent`; ConcreteElement `WorkItem`, `Stage`; ObjectStructure `WorkflowInstance` | to agree (Sloth fits: audit views) | Interface published (`accept`) | `accept(WorkflowVisitor*)` is in the header, matching the design. |
| 13 | Proxy | Structural | Subject `WorkflowComponent`; RealSubject `WorkItem`, `Stage`; Proxy `AccessControlProxy` + `AccessPolicy` | to agree (Greed fits: roles) | Not started | `WorkflowComponent.h` has no `proxyRequest()`/`request()` operation, so the proxy has nothing to guard yet. |
| 14 | Iterator | Behavioural | Iterator `WorkflowIterator`; ConcreteIterator `AvailableWorkIterator`, `HistoryIterator`; Aggregate `WorkflowComponent` | to agree | Not started | `createIterator()` is on the diagram but not in the header. |

## 3. Gap that needs a team decision: a second creational pattern

Builder is the only creational pattern, and the spec requires two. I do not add one alone, because the choice changes someone else's code. These options solve a real TIVIDY problem rather than adding a demo class:

1. **Prototype (recommended), owner Pride.** Starting an instance must give it its *own* tree of `WorkItem`s and `Stage`s copied from the definition, which is exactly the instance-isolation requirement. `WorkflowComponent::clone()` (Prototype), implemented by `WorkItem`, `Stage` and `TaskDecorator` (ConcretePrototypes), called by `WorkflowEngine::startInstance` (Client). It is not in the "five from the list" set, but that rule is already met.
2. **Abstract Factory, owners Greed and Gluttony.** `IndustryFactory` creates a matching family per industry (definition builder, routing rule, assignment rule, connector stub). This directly shows "both industries use the same engine, plugged in through one interface".
3. **Singleton** is on the listed set but is hard to justify meaningfully here, so I do not recommend it.

Action: group leader to pick one this week. The chosen sin sends Envy the justification.

## 4. Updated justification: Strategy (routing, decisions and dependencies). Envy

**Design problem.** When a work item completes, TIVIDY must decide what becomes available next. Every industry decides this differently: a delivery company branches on stock and distance, an insurer on claim value. Some steps run in sequence, some split into parallel work, some wait until several earlier steps are done. If this logic sat in `WorkflowInstance`, the generic engine would need editing for every new process, and a second industry would mean a second engine.

**Participants (actual C++ classes in `code/routing`, `code/progression` and `code/data`).**

- Strategy: `RoutingRule`: `determineNextWork(completedItem, instanceData) : RoutingDecision`.
- ConcreteStrategy: `SequentialRouting` (fixed paths; several successors = parallel split) and `DataBasedRouting` (adds decision points whose branches are guarded by `RoutingCondition`s; falls back to the fixed path elsewhere).
- Context: `WorkflowInstance`, which delegates to its own `ProgressionTracker`. The tracker calls only the `RoutingRule` interface and never knows which concrete rule it has.
- Supporting classes (not GoF roles): `DependencyGraph` (prerequisites, AND-join, cycle check), `KeyValuePayload` (each instance's own data as named fields; a concrete subclass of the team's abstract `DataPayload`), `RoutingDecision` and `ProgressionResult` (what happened, for history and the interface).

**Collaboration.** The definition owns one `RoutingRule` and one `DependencyGraph`, shared by all its instances. Each instance owns a `ProgressionTracker`. On `complete(item, data)` the tracker asks the rule for a `RoutingDecision`. For each nominated item it asks `DependencyGraph::isSatisfied` whether the instance has completed that item's prerequisites. Satisfied items become available; the rest wait and are released when their last prerequisite completes. The instance then calls `makeAvailable` on its own `WorkItem`s (State pattern) and assigns them (`AssignmentRule`). Sequence Diagram 1 shows this.

**What varies.** The routing algorithm and the decision guards, supplied per industry when the definition is built. A new kind of routing is one new ConcreteStrategy. A new decision is data (`addBranch`), not a new class. `ProgressionTracker`, `DependencyGraph` and `WorkflowInstance` do not change between industries.

**Why Strategy fits.** The engine needs a decision whose *algorithm* differs per industry while the moment it is asked (after completion) and the use of its answer stay fixed. Branches are an ordered list of guard objects, not an if/else chain in the engine, which satisfies the "no large centralised branching" rule. Because rules are stateless and return names rather than pointers into one instance, a single rule is safely shared by every instance, so two instances of the same definition can take different branches without affecting each other. Tests prove this.

**Changes since Practical 6** (decision log D-04 to D-07): names instead of `WorkItem*`; returns a `RoutingDecision` rather than `List<WorkItem*>`; `routingConditions : Map<String,String>` became an ordered list of `RoutingCondition` guards; `DataBasedRouting` extends `SequentialRouting`; `DependencyGraph` and `ProgressionTracker` are new.

**Evidence.** `tests/envy/test_routing.cpp`, `tests/envy/test_dependencies.cpp`, `tests/envy/test_progression.cpp`; pattern class diagram `docs/envy/uml/PatternClassDiagram_Strategy_Routing.png`.

## 5. Report section (for Lust's PDF)

Use sections 1, 2 and 4 above as the "Updated pattern roles and justifications" section. Re-run the audit after the last engine pull request and before the PDF is assembled, so the Status column says "Executing" for at least 10 rows.
