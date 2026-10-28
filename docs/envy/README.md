# Envy: dependencies, decisions, branching and progression

My part of the TIVIDY final project, as assigned in *Seven Sins of TIVIDY: final*. It's matched to the code on `integration-Code` (decisions D-09 and D-10).

| Assigned | Where it is |
|---|---|
| **Code:** dependencies between work items | `code/progression/DependencyGraph.*` |
| **Code:** decisions and routing branches | `code/routing/RoutingRule.h`, `SequentialRouting.*`, `DataBasedRouting.*`, `RoutingCondition.*` |
| **Code:** per-instance data for routing decisions | `code/data/KeyValuePayload.*` (extends the team's `code/DataPayload.h`) |
| **Code:** progression through the workflow once work completes | `code/progression/ProgressionTracker.*` |
| **Tests:** routing takes the right branch; dependencies block work until met | `tests/envy/test_routing.cpp`, `test_dependencies.cpp`, `test_progression.cpp`, `test_integration.cpp` (40 cases, doctest) |
| **Diagram:** Activity Diagram 1 + Sequence Diagram 1, updated to the code | `docs/envy/uml/*.puml` (source) + `*.png` (exports) |
| **Tool:** pattern audit | `docs/envy/patterns.md` §1–3, plus `docs/envy/uml/PatternClassDiagram_Strategy_Routing.png` |
| **Report:** updated pattern roles and justifications | `docs/envy/patterns.md` §4–5 |
| Decisions log entries | `docs/envy/decisions.md` (D-04 to D-10) |

All code is C++11, has Doxygen comments, and contains no industry-specific logic. Industries supply paths, decisions and dependencies as data when they build their definition.

## Build and test

Needs CMake 3.10+ and a C++11 compiler. The test framework is doctest (single header, `tests/envy/doctest.h`, MIT licence, https://github.com/doctest/doctest).

```
cmake -S tests/envy -B build-envy
cmake --build build-envy
cd build-envy && ctest --output-on-failure        # or run ./envy_tests directly
```

Without CMake:

```
g++ -std=c++11 -Icode -Icode/workflow -Icode/data -Icode/routing -Icode/progression -Itests/envy \
    code/data/*.cpp code/routing/*.cpp code/progression/*.cpp tests/envy/*.cpp -o envy_tests
./envy_tests
```

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

## What I need from the others

- **Pride:** `WorkflowDefinition` getters for the routing rule, dependency graph and entry items. Also: fix the `chnageState` and `WorkflowDefnition.h` typos, and the other header-vs-diagram mismatches listed in `docs/envy/patterns.md` §2.
- **Sloth / Pride:** `InstanceSnapshot` should store the tracker's available, waiting and completed lists, so rollback restores progress too.
- **Greed / Gluttony:** build each industry's paths, decisions and dependencies with `addPath`, `addBranch`/`setDefaultBranch` and `addDependency`. Use at least one decision and one dependency per industry (checklist item f05).
- **Lust:** `RoutingDecision::description` and `ProgressionResult` (available and waiting items) are ready to print. `unmetPrerequisites(item)` explains why work is blocked.
- **Group leader:** pick the second creational pattern (`docs/envy/patterns.md` §3). The count fails without it.

## Diagrams in Visual Paradigm

The brief wants editable VP files. Redraw AD1 and SD1 in the shared VP project from the PNG exports here (same nodes, guards and messages), then commit the `.vpp` and an export together, as the team rules say. The `.puml` files are the agreed source until then.

## Notes from integrating with `integration-Code`

These are in other members' files. I haven't changed them, but they stop the full engine from compiling:

- `code/workflow/WorkItem.cpp`: `getName()` returns `name`, but the member is `itemName`. `Event.h` is included but doesn't exist yet.
- `code/workflow/WorkflowDefinition.cpp`: `getRootStage()` returns `WorkFlowComponent*` (capital F).
- `code/workflow/WorkflowInstance.*`: `InstanceSnapshot.h` doesn't exist yet. `start`, `isComplete`, `createSnapshot` and `restore` are pure virtual in the header but defined in the `.cpp`. The destructor is `= default` in the header and also defined. `currentStates` is used but not declared.
- The root folder still has older copies of the state and workflow files. `code/` is the up-to-date layout.
