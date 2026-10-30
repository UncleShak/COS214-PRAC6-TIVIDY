# TIVIDY GoF patterns: audit, participant mapping and justifications

Owner: Envy (pattern audit). Status as of 30 Oct 2026 (`integration-Code` at `282c3b1`, plus Envy's Visitor and Iterator).

The final project spec (section 4.1) asks for at least 10 distinct, meaningful GoF patterns that **execute** in the system, with at least 2 creational, 2 structural and 2 behavioural, and at least 5 from the listed set (Observer, Iterator, Mediator, Command, Adapter, Chain of Responsibility, Builder, Interpreter, Bridge, Facade, Visitor, Proxy, Singleton, Flyweight). The mappings and pattern class diagrams must describe the actual C++ classes.

This file is checked against the code each time a sin's pull request lands. Every sin sends Envy the justification for any pattern their code implements; Envy updates the row and section below.

## 1. Count check

| Rule | Practical 6 design | Executing in code today | Result |
|---|---|---|---|
| At least 10 distinct patterns | 14 | 0 run inside a working engine yet. Coded: Strategy (routing, assignment), Decorator, Command, Visitor, Iterator, so 5 distinct. Interfaces only: Composite, State, Facade, Memento, Observer | Design passes; code not there yet. Blocker: `WorkItem.cpp` cannot compile (empty State headers, no `Event.h`) |
| At least 2 creational | **1** (Builder) | 0 | **Fails. Needs a team decision (section 3)** |
| At least 2 structural | 5 (Composite, Decorator, Adapter, Proxy, Facade) | 1 coded (Decorator) | Design passes |
| At least 2 behavioural | 8 (State, Strategy, Observer, Chain of Responsibility, Memento, Command, Visitor, Iterator) | 4 coded (Strategy, Command, Visitor, Iterator) | Design passes |
| At least 5 from the listed set | 9 (Observer, Iterator, Command, Adapter, Chain of Responsibility, Builder, Facade, Visitor, Proxy) | 3 coded (Command, Visitor, Iterator) | Design passes; code needs 2 more |

"Executing" means: the class exists in C++, it is called by the running engine, and a test exercises it. A header with pure virtual functions only counts as "interface published".

## 2. Pattern-by-pattern audit

Status key: **Executing** · **Coded + unit-tested** (classes and tests exist, not yet called by a running engine) · **Coded** · **Interface published** (header only) · **Not started**.

| # | Pattern | Category | GoF roles → TIVIDY classes | Code owner | Status | Mismatches between code and Practical 6 diagram |
|---|---|---|---|---|---|---|
| 1 | Strategy (routing) | Behavioural | Strategy `RoutingRule`; ConcreteStrategy `SequentialRouting`, `DataBasedRouting`; Context `WorkflowInstance` via `ProgressionTracker` | Envy | **Coded + unit-tested** (`tests/envy`) | Changed on purpose, see D-04 to D-10 and section 4 |
| 1b | Strategy (assignment) | Behavioural | Strategy `AssignmentRule`; ConcreteStrategy `RoleBasedAssignment`, `WorkloadBasedAssignment`; Context `WorkflowInstance` | Greed | **Coded + unit-tested** (`tests/greed`) | Tests use `WorkItem`, so they cannot link until `WorkItem.cpp` compiles |
| 2 | Composite | Structural | Component `WorkflowComponent`; Leaf `WorkItem`; Composite `Stage` | Pride | Interface published | Diagram's `WorkflowComponent` has `createIterator()`, `proxyRequest()`, `accept()`; header has `getName()`, `add()`, `remove()`, `accept()`. `Stage` has no header yet. Header puts `add`/`remove` on the Component (transparent Composite), so `WorkItem` must implement them, likely by refusing. Record that choice. |
| 3 | State | Behavioural | Context `WorkItem`; State `WorkItemState`; ConcreteState `CreatedState`, `AvailableState`, `AssignedState`, `StartedState`, `CompletedState`, `CancelledState` | Pride | Interface published | Header has one operation per trigger (`makeAvailable`, `assign`, `start`, `complete`, `reject`, `cancel`, `escalate`), but the diagram shows a single `handle()`. The header is the better design, so update the diagram. The header has `reject`/`escalate` but there are no `RejectedState`/`EscalatedState`. Either add them or say which state each one moves to. The diagram has the typo `AssignedSate`. `WorkItem.h` declares `chnageState` (typo for `changeState`). |
| 4 | Observer | Behavioural | Subject `EventSource`; ConcreteSubject `WorkItem`; Observer `EventListener`; ConcreteObserver `WorkflowInstance`, `NotificationObserver`, `MonitoringObserver` | Sloth | Interface published (partly) | `WorkItem.h` has `notifyListeners()` but no `attach`/`detach`, and does not derive from `EventSource`. `EventSource.h` and `EventListener.h` are not written yet. |
| 5 | Memento | Behavioural | Originator `WorkflowInstance`; Memento `InstanceSnapshot`; Caretaker `HistoryManager` | Sloth / Pride | Interface published (partly) | `createSnapshot()`/`restore()` match the diagram. `InstanceSnapshot.h` is missing, so `WorkflowInstance.h` does not compile yet. The snapshot must now also hold the instance's `ProgressionTracker` progress (available, waiting and completed items). Otherwise a rollback restores work-item states but leaves the routing frontier wrong. |
| 6 | Facade | Structural | Facade `WorkflowEngine`; subsystems builder, instances, history, events | Pride | Interface published | Header: `startInstance`, `completeWorkItem`, `assignWork`. Diagram: `deployWorkflow`, `startInstance`, `getAvailableWork`, `completeWorkItem`. `deployWorkflow` and `getAvailableWork` are missing, and Lust's interface needs `getAvailableWork` and a history query. `assignWork` is not on the diagram. |
| 7 | Builder | Creational | Builder `WorkflowBuilder`; ConcreteBuilder `ScenarioBuilder` (one per industry); Director `EngineConfigurator`; Product `WorkflowDefinition` | Greed / Gluttony (industry builders), Pride (product) | Interface published (product only) | Header `WorkflowDefinition` has `getRootStage()`, `getVersion()`; diagram has `getRoot()` and attributes `rootComponent`, `definitionVersion`. Pick one name. The product also needs `getRoutingRule()`, `getDependencies()` and `getEntryItems()` so an instance can build its `ProgressionTracker`. `buildDependency()` should fill a `DependencyGraph`. File name typo: `WorkflowDefnition.h`. |
| 8 | Decorator | Structural | Component `WorkflowComponent`; ConcreteComponent `WorkItem`; Decorator `TaskDecorator`; ConcreteDecorator `ValidationDecorator`, `AuditDecorator`, `DeadlineDecorator`, `RetryDecorator` | Gluttony (concrete decorators), Pride (base) | **Coded** (`code/decorator`) | `request()` added to `WorkflowComponent`. `TaskDecorator::accept` forwards to the wrapped item, so Visitor sees the real `WorkItem`. Old copy `code/TaskDecorator.h` still exists next to `code/decorator/TaskDecorator.h`; delete one |
| 9 | Chain of Responsibility | Behavioural | Handler `ApprovalHandler`; ConcreteHandler `RoleBasedApprover`; Client `WorkItem` | Wrath | Not started | none yet |
| 10 | Command | Behavioural | Command `Command`; ConcreteCommand `AssignCommand`, `StartCommand`, `CompleteCommand`, `RejectCommand`, `CancelCommand`, `EscalateCommand`; Receiver `WorkItem`; Invoker `CommandLog` (for `WorkflowInstance`) | Gluttony | **Coded** (`code/command`); exercised by Envy's Iterator tests | Invoker is `CommandLog`, which `WorkflowInstance` should own |
| 11 | Adapter | Structural | Target `Connector`; Adapter `ExternalSystemAdapter`; Adaptee `ExternalSystem` (stubbed) | Greed / Gluttony (industry stubs) | Not started | none yet |
| 12 | Visitor | Behavioural | Visitor `WorkflowVisitor`; ConcreteVisitor `ProgressReportVisitor`, `OverdueWorkVisitor`, `AuditReportVisitor`; Element `WorkflowComponent`; ConcreteElement `WorkItem`, `Stage`; ObjectStructure `WorkflowInstance` | Envy | **Coded + unit-tested** (`code/visitor`, `tests/envy/test_visitor.cpp`) | Needs from Pride: `WorkItem::accept` body (one line) and `Stage`. Tested with stand-ins until then (D-11) |
| 13 | Proxy | Structural | Subject `WorkflowComponent`; RealSubject `WorkItem`, `Stage`; Proxy `AccessControlProxy` + `AccessPolicy` | to agree (Greed fits: roles) | Not started | `WorkflowComponent.h` has no `proxyRequest()`/`request()` operation, so the proxy has nothing to guard yet. |
| 14 | Iterator | Behavioural | Iterator `WorkflowIterator<T>`; ConcreteIterator `AvailableWorkIterator`, `HistoryIterator`; Aggregate `ProgressionTracker` (`createIterator()`) and the history entries (`CommandLog`, Sloth's history) | Envy | **Coded + unit-tested** (`code/iterator`, `tests/envy/test_iterator.cpp`) | Aggregate moved from `WorkflowComponent::createIterator()` to `ProgressionTracker` and history (D-12) |

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

## 6. Justification: Visitor (monitoring and reporting). Envy

**Design problem.** A running workflow has to be inspected in different ways: how far it has got, what nobody has acted on in time, who holds which work. Every organisation wants different views, and new ones keep being added. Each view must walk the whole tree of stages and work items and do something different for each kind of element. If every report were an operation on `WorkflowComponent`, `WorkItem` and `Stage`, the core model would be edited for every new report, which the spec rules out ("industry-specific rules kept separate from the generic engine").

**Participants (C++ classes in `code/visitor`).**

- Visitor: `WorkflowVisitor`, which declares `visitWorkItem(WorkItem*)` and `visitStage(Stage*)`.
- ConcreteVisitor:
  - `ProgressReportVisitor` counts work by `WorkItemState::getStateName()` and gives the percentage complete (cancelled work excluded).
  - `OverdueWorkVisitor` lists work in the "Escalated" state. In the team's State pattern, that's work nobody acted on in time, and `DeadlineExceeded` leads to it.
  - `AuditReportVisitor` writes one line per item: name, state, assigned participant and required role.
- Element: `WorkflowComponent::accept(WorkflowVisitor*)` (Pride's header).
- ConcreteElement: `WorkItem` (calls `visitWorkItem(this)`) and `Stage` (calls `visitStage(this)`, then `accept` on each child).
- ObjectStructure: `WorkflowInstance`, which owns the root of the work tree. Client: the interface (Lust) or a monitoring service.

**Collaboration.** The client creates a visitor and calls `root->accept(&visitor)`. Each element calls the visit operation for its own type (double dispatch), so no code checks types. `TaskDecorator::accept` (Gluttony) forwards to the wrapped component, so decorated work is reported as the real `WorkItem`. The visitor collects results as it goes, and the client reads them, for example `summary()`, `overdueNames()` or `text()`.

**What varies.** The set of reports, which grows over time and differs per organisation. The element kinds don't vary: whatever the industry, the engine only has work items and stages.

**Why Visitor.** The element classes are few and stable, while the operations over them keep changing. That's the case the pattern is for. A new report is one new ConcreteVisitor; `test_visitor.cpp` adds one inside a test without touching `WorkItem` or `Stage`. The trade-off is that adding a new element kind would mean a new `visit` operation on every visitor, which is acceptable because the element kinds are fixed by the generic design.

**Evidence.** `tests/envy/test_visitor.cpp` (10 cases): double dispatch, nested stages, decorated items, each concrete visitor, extension by a new visitor, null input.

## 7. Justification: Iterator (available work and history). Envy

**Design problem.** The interface, the reports and the engine must step through an instance's available work and through its history, without knowing how either is stored. The tracker keeps a `vector`, `CommandLog` keeps `unique_ptr<Command>`s, and Sloth's history will have its own structure. Exposing those containers would tie every caller to them. Looping over a live list while completing work also changes that list underneath the loop.

**Participants (C++ classes in `code/iterator`).**

- Iterator: `WorkflowIterator<T>`, with `first()`, `next()`, `isDone()` and `currentItem()` (GoF form, as in the COS 214 slides).
- ConcreteIterator: `AvailableWorkIterator` (names of available work, in the order it became available) and `HistoryIterator` (history entries oldest or newest first, optionally only those mentioning one work item).
- Aggregate: `ProgressionTracker`, whose `createIterator()` is the factory method. History comes as entries from `CommandLog::describeAll()`, `AuditDecorator`'s `History`, or Sloth's history class.
- Client: the interface (Lust), reports, `WorkflowInstance`.

**Collaboration.** A client asks the aggregate for an iterator and loops with `first`/`isDone`/`currentItem`/`next`. Each iterator copies what it traverses when it's created, so completing work or recording history mid-loop is safe; `test_iterator.cpp` completes work inside the loop. One loop works for every concrete iterator.

**What varies.** How each collection is stored, and the order and filter in which a client wants to see it.

**Why Iterator.** It separates traversal from storage, which is the problem the pattern solves. `createIterator()` is also an example of Factory Method, as the slides note.

**Evidence.** `tests/envy/test_iterator.cpp` (8 cases): tracker iterator, safe completion mid-loop, restart and bounds, empty traversal, history order and filtering, iteration over the real `CommandLog` and `AuditDecorator` history, polymorphic loop.

