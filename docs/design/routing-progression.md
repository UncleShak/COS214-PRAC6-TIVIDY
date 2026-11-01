# Envy: dependencies, decisions, branching and progression

My part of the TIVIDY final project, as assigned in *Seven Sins of TIVIDY: final*, plus the Visitor and Iterator patterns. It's matched to the code on `integration-Code` (decisions D-09 to D-13).

| Assigned | Where it is |
|---|---|
| **Code:** dependencies between work items | `src/execution/progression/DependencyGraph.*` |
| **Code:** decisions and routing branches | `src/execution/routing/RoutingRule.h`, `SequentialRouting.*`, `DataBasedRouting.*`, `RoutingCondition.*` |
| **Code:** per-instance data for routing decisions | `src/workflow/model/KeyValuePayload.*` (extends the team's `src/workflow/model/DataPayload.h`) |
| **Code:** progression through the workflow once work completes | `src/execution/progression/ProgressionTracker.*` |
| **Pattern:** Visitor (progress, overdue and audit reports) | `src/monitoring/reports/` |
| **Pattern:** Iterator (available work, history) | `src/execution/traversal/`, `ProgressionTracker::createIterator()` |
| **Tests:** routing takes the right branch; dependencies block work until met | `tests/unit/test_routing.cpp`, `test_dependencies.cpp`, `test_progression.cpp`, `test_integration.cpp`, `test_visitor.cpp`, `test_iterator.cpp` (58 cases, doctest) |
| **Diagram:** Activity Diagram 1 + Sequence Diagram 1, updated to the code | `docs/diagrams/uml/*.puml` (source) + `*.png` (exports) |
| **Tool:** pattern audit | `docs/design/patterns.md` §1–3, plus `docs/diagrams/uml/PatternClassDiagram_*.png` (Strategy, Visitor, Iterator) |
| **Report:** updated pattern roles and justifications | `docs/design/patterns.md` §4–7 |
| Decisions log entries | `docs/design/decisions.md` (D-04 to D-13) |

All code is C++11, has Doxygen comments, and contains no industry-specific logic. Industries supply paths, decisions and dependencies as data when they build their definition.

## Build and test

Needs CMake 3.10+ and a C++11 compiler. The test framework is doctest (single header, `tests/support/doctest.h`, MIT licence, https://github.com/doctest/doctest).

```
cmake -S tests/unit -B build-envy
cmake --build build-envy
cd build-envy && ctest --output-on-failure        # or run ./envy_tests directly
```

Without CMake, from the repository root:

```
C=code
g++ -std=c++11 -I$C -I$C/workflow -I$C/state -I$C/participants -I$C/decorator -I$C/command \
    -I$C/core -I$C/data -I$C/routing -I$C/progression -I$C/visitor -I$C/iterator \
    -Itests/unit -Itests/support \
    $C/data/*.cpp $C/routing/*.cpp $C/progression/*.cpp $C/visitor/*.cpp $C/iterator/*.cpp \
    $C/participants/*.cpp $C/decorator/TaskDecorator.cpp $C/decorator/AuditDecorator.cpp \
    $C/command/*.cpp tests/support/WorkItemStandIn.cpp tests/unit/*.cpp -o envy_tests
./envy_tests
```

The Visitor and Iterator tests run with the team's real `TaskDecorator`, `AuditDecorator`, commands and `Participant`. Because `src/workflow/model/WorkItem.cpp` can't compile yet, they link a test-only stand-in for it; see `tests/support/README.md`.

When Wrath adds the root `CMakeLists.txt`, move the `envy_engine` sources into the team engine library and add `envy_tests` to the team test target.

## How it plugs into the team's classes

Everything below uses the team's names from `integration-Code`: `WorkItem`, `WorkItem::getName()`, `WorkItem::makeAvailable()/complete()/reject()/cancel()`, `WorkItemState::getStateName()`, and `WorkflowInstance`'s `DataPayload* payload`.

| Team's lifecycle (State pattern) | Call into Envy's tracker | What the tracker gives back |
|---|---|---|
| `WorkflowInstance::start()` | `tracker.start()` | names to `makeAvailable()` (`CreatedState -> AvailableState`) |
| `item->complete()` (`-> CompletedState`) | `tracker.complete(item, payload)` | `ProgressionResult` with `outcome == "Completed"` |
| `item->reject()` (`-> RejectedState`) | `tracker.reject(item, payload)` | routes to the recovery branch; a rejected item doesn't satisfy dependencies |
| `item->cancel()` (`-> CancelledState`) | `tracker.cancel(item)` | removed from available or waiting, so the instance can still finish |

Items reached whose prerequisites aren't met stay in `CreatedState` (listed in `result.waiting`) until the tracker releases them. `CompletedState`, `RejectedState` and `CancelledState` are final, so an item reached again by a retry loop is listed in `result.reopened`, and the instance creates a fresh `WorkItem` for it.

```cpp
// WorkflowDefinition (Pride): built by WorkflowBuilder
RoutingRule*             routingRule;   // e.g. a DataBasedRouting
DependencyGraph          dependencies;  // filled by buildDependency(dependent, prerequisite)
std::vector<std::string> entryItems;    // where every instance starts

// Creating an instance (industry / Facade)
KeyValuePayload order;
order.set("orderValid", "yes");
WorkflowInstance* inst = new WorkflowInstance(definition, &order);   // clones the payload

// WorkflowInstance (Pride): one tracker per instance, next to `DataPayload* payload`
ProgressionTracker tracker;  // tracker(*def->routingRule, def->dependencies, def->entryItems)

void WorkflowInstance::start() {
    std::vector<std::string> ready = tracker.start();
    for (size_t i = 0; i < ready.size(); ++i) itemNamed(ready[i])->makeAvailable();
}

void WorkflowInstance::completeWorkItem(WorkItem* item) {
    item->complete();                                             // StartedState -> CompletedState
    apply(tracker.complete(item, payload));                       // WorkItem* and DataPayload* as-is
}

void WorkflowInstance::rejectWorkItem(WorkItem* item) {
    item->reject();                                               // StartedState -> RejectedState
    apply(tracker.reject(item, payload));
}

void WorkflowInstance::apply(const ProgressionResult& r) {
    for (size_t i = 0; i < r.reopened.size(); ++i) replaceWithFreshItem(r.reopened[i]);   // final states
    for (size_t i = 0; i < r.madeAvailable.size(); ++i) {
        WorkItem* next = itemNamed(r.madeAvailable[i]);
        next->makeAvailable();                                    // CreatedState -> AvailableState
        // Participant* p = assignmentRule->assignParticipant(next, participants);   // Greed
        // if (p) { next->assignTo(p); next->assign(); p->workAssigned(); }
    }
    // history: record r.outcome + r.decision.description            // Sloth
    // if (r.instanceComplete) mark complete; if (tracker.isStalled()) escalate   // Wrath
}
```

## Visitor and Iterator: how to use them

```cpp
// Visitor: a report over the whole work tree (root = the instance's root Stage)
ProgressReportVisitor progress;
root->accept(&progress);
std::cout << progress.summary();          // "4 work items: 1 Available, 2 Completed, 1 Started (50% complete)"

OverdueWorkVisitor overdue;               // escalated work, for Wrath's escalation chain
root->accept(&overdue);
for (WorkItem* w : overdue.overdueItems()) { /* ... */ }

AuditReportVisitor audit;                 // "ReceiveOrder | Started | Thandi | role: Clerk"
root->accept(&audit);
std::cout << audit.text();

// Iterator: available work and history, without knowing how they are stored
std::unique_ptr<WorkflowIterator<std::string> > work = tracker.createIterator();
for (work->first(); !work->isDone(); work->next()) show(itemNamed(work->currentItem()));

HistoryIterator history(commandLog.describeAll(), HistoryIterator::NewestFirst, "ReceiveOrder");
for (history.first(); !history.isDone(); history.next()) std::cout << history.currentItem() << "\n";
```

A new report is a new class that implements `WorkflowVisitor`. Nothing else changes.

## What I need from the others

- **Pride (Visitor):** add `void WorkItem::accept(WorkflowVisitor* v) { v->visitWorkItem(this); }` to `WorkItem.cpp`. Add `Stage`, whose `accept` calls `visitStage(this)` then `accept` on each child. Write the six empty State headers in `src/workflow/lifecycle/`, so `WorkItem.cpp` compiles. Then the test stand-ins in `tests/support/` can be deleted.
- **Sloth (Iterator):** give your history class a way to read its entries in order, for example `std::vector<std::string> entries() const`, so `HistoryIterator` can walk it. `Event.h` is needed before `WorkItem.cpp` compiles.
- **Lust (interface):** use `tracker.createIterator()` for "show available work", `HistoryIterator` for "view history", and the three visitors for progress, overdue and audit screens.
- **Wrath:** `OverdueWorkVisitor` lists escalated work, which your escalation chain can use.

- **Pride:** `WorkflowDefinition` getters for the routing rule, dependency graph and entry items. Also: fix the `chnageState` and `WorkflowDefnition.h` typos, and the other header-vs-diagram mismatches listed in `docs/design/patterns.md` §2.
- **Sloth / Pride:** `InstanceSnapshot` should store the tracker's available, waiting and completed lists, so rollback restores progress too.
- **Greed / Gluttony:** build each industry's paths, decisions and dependencies with `addPath`, `addBranch`/`setDefaultBranch` and `addDependency`. Use at least one decision and one dependency per industry (checklist item f05).
- **Lust:** `RoutingDecision::description` and `ProgressionResult` (available and waiting items) are ready to print. `unmetPrerequisites(item)` explains why work is blocked.
- **Group leader:** pick the second creational pattern (`docs/design/patterns.md` §3). The count fails without it.

## Diagrams in Visual Paradigm

The brief wants editable VP files. Redraw AD1 and SD1 in the shared VP project from the PNG exports here (same nodes, guards and messages), then commit the `.vpp` and an export together, as the team rules say. The `.puml` files are the agreed source until then.

## Notes from integrating with `integration-Code`

These are in other members' files. I haven't changed them, but they stop the full engine from compiling (checked 30 Oct 2026):

- `src/workflow/lifecycle/`: `CreatedState.h`, `AvailableState.h`, `AssignedState.h`, `StartedState.h`, `CompletedState.h` and `EscalatedState.h` are empty files, so the State pattern can't compile.
- `src/workflow/model/WorkItem.cpp`: `Event.h` is included but doesn't exist yet. `accept()` and `notifyListeners()` are declared but have no body.
- `src/workflow/model/WorkflowInstance.*`: `InstanceSnapshot.h` doesn't exist yet. `start`, `isComplete`, `createSnapshot` and `restore` are pure virtual in the header but defined in the `.cpp`. The destructor is `= default` in the header and also defined. `currentStates` is used but not declared.
- `src/tasks/TaskDecorator.h` is an older copy of `src/tasks/TaskDecorator.h`.
