# Decisions log entries: Envy (final project)

Add these to `docs/decisions.md` in the same pull request as the code. They continue from D-03 in Practical 6. If other sins have taken D-04 onward by the time this merges, renumber them in order.

## D-04 · 2026-10-26 · Envy
**Question:** What should a routing rule receive and return, given that one definition's rule serves many instances?
**Before:** `RoutingRule::determineNextWork(completedItem : WorkItem*, instanceData : DataPayload*) : List<WorkItem*>`.
**Change:** `determineNextWork(const std::string& completedItem, const DataPayload& instanceData) const : RoutingDecision`. The rule works with work-item names, and each instance resolves them to its own `WorkItem`s. `RoutingDecision` carries the next items and a readable description of the branch taken.
**Why:** The rule belongs to the definition and is shared by every instance. Returning `WorkItem*` would tie a shared rule to one instance's objects and break instance isolation. The description lets the interface and history show the routing that happened, as section 5 of the spec requires.
**Diagrams:** Class diagram (`RoutingRule`, `SequentialRouting`, `DataBasedRouting`), Sequence Diagram 1.
**Code:** `RoutingRule.h`, `SequentialRouting.*`, `DataBasedRouting.*`; `tests/envy/test_routing.cpp`.

## D-05 · 2026-10-26 · Envy
**Question:** Where does "what is available next, and is the instance finished?" live?
**Before:** Inside `WorkflowInstance::completeWorkItem`, as a loop over the routing result.
**Change:** New class `ProgressionTracker`, one per instance, owned by `WorkflowInstance`. It holds that instance's available, waiting and completed items. `complete(item, data)` routes, checks dependencies and returns a `ProgressionResult`. The instance applies the result through `WorkItemState::makeAvailable`.
**Why:** Keeps `WorkflowInstance` small and lets progression be tested without the whole engine. It also separates the flow of work (tracker) from each item's lifecycle (`WorkItemState`), so the State diagram is unchanged. Invalid completions (work not available, waiting or already done) are refused with `std::logic_error` and leave progress unchanged.
**Diagrams:** Class diagram (new class, composition from `WorkflowInstance`), Activity Diagram 1, Sequence Diagram 1. The `InstanceSnapshot` (Memento) must also hold tracker progress.
**Code:** `ProgressionTracker.*`; `tests/envy/test_progression.cpp`.

## D-06 · 2026-10-26 · Envy
**Question:** How are dependencies between work items represented and enforced?
**Before:** Only `WorkflowBuilder::buildDependency(dependentItem, prerequisite)`; no class held the result.
**Change:** New class `DependencyGraph`, owned by `WorkflowDefinition` and filled by `buildDependency`. An item needs all its prerequisites complete (AND-join). Dependencies that would create a cycle are refused when built. An item reached by routing whose prerequisites are unmet *waits* and is released when the last one completes. If work waits while nothing is available, `isStalled()` reports it so the engine can escalate.
**Why:** Dependencies are part of the definition, but whether they are met is per instance. Separating the two keeps instances independent. Refusing cycles at build time stops a definition that could never finish.
**Diagrams:** Class diagram, Activity Diagram 1.
**Code:** `DependencyGraph.*`, `ProgressionTracker.*`; `tests/envy/test_dependencies.cpp`, `tests/envy/test_progression.cpp`.

## D-07 · 2026-10-26 · Envy
**Question:** How are decision branches expressed without an if/else chain in the engine?
**Before:** `DataBasedRouting.routingConditions : Map<String, String>`, a sibling of `SequentialRouting`.
**Change:** `DataBasedRouting` extends `SequentialRouting`. Each decision point is an ordered list of branches guarded by `RoutingCondition` objects plus an optional default. The first branch whose guard holds wins. Industries can pass their own guard function. No match and no default throws `std::runtime_error`.
**Why:** A string map cannot express order, numeric comparisons ("distance > 50 km") or a default branch. Most workflows are mostly fixed paths with a few decisions, so reusing the fixed-path routing avoids duplicating it. Guards are data, not engine branches.
**Diagrams:** Class diagram, Strategy pattern class diagram, Activity Diagram 1.
**Code:** `DataBasedRouting.*`, `RoutingCondition.*`; `tests/envy/test_routing.cpp`.

## D-08 · 2026-10-26 · Envy
**Question:** Do Activity Diagram 1 and Sequence Diagram 1 still match the system?
**Before:** AD1 (from Gluttony in Practical 6) evaluated dependencies once, drew a fixed two-item fork, and its "Wait" branch led straight to "Start WorkItem". SD1's outer `alt` had no guards, `StartedState` used `handle()`, and routing returned `WorkItem*`s.
**Change:** AD1 now has swimlanes for the engine, instance, tracker and routing rule; decision guards in order with a default; per-item dependency checks with a waiting path; release of waiting work; refusal of invalid completion; and stalled vs completed final outcomes. SD1 now guards the proxy `alt`, uses `WorkItemState::complete(item)` from Pride's header, and adds `ProgressionTracker`, `RoutingCondition` and `DependencyGraph`.
**Why:** The final spec requires all diagrams to agree with the C++ code. AD1 moved to Envy in the final split.
**Diagrams:** Activity Diagram 1, Sequence Diagram 1.
**Code:** none (diagram-only change).

## D-09 · 2026-10-28 · Envy
**Question:** How does Envy's per-instance data fit the team's `DataPayload` on `integration-Code`?
**Before:** Envy had a concrete `DataPayload` (named text fields). `integration-Code` has an abstract `DataPayload` (`getPayloadType()`, `clone()`) that `WorkflowInstance` holds as `DataPayload*` and clones for instance isolation and Memento snapshots. Same name, different classes.
**Change:** Kept the team's abstract `DataPayload` unchanged. Envy's class is now `KeyValuePayload : public DataPayload` (`code/data/`), implementing `getPayloadType()` ("KeyValue") and `clone()`. `RoutingRule`, `RoutingCondition` and `ProgressionTracker` take `const DataPayload&`, so `WorkflowInstance` passes `*payload` straight in. The built-in guards (`equals`, `greaterThan`, ...) read fields from a `KeyValuePayload`. For any other payload type they are false and the decision takes its default branch. Industries with their own payload type pass a custom predicate.
**Why:** No edits to other members' classes, one data type across the engine, and cloning works for both instance isolation and Memento.
**Diagrams:** Strategy pattern class diagram (adds `KeyValuePayload` under `DataPayload`).
**Code:** `code/data/KeyValuePayload.*`, `code/routing/RoutingCondition.cpp`; `tests/envy/test_routing.cpp`, `tests/envy/test_integration.cpp`.

## D-10 · 2026-10-28 · Envy
**Question:** Does progression line up with the team's `WorkItem` and `WorkItemState` on `integration-Code`?
**Before:** The tracker only knew "completed". The team's State pattern also has `RejectedState` (finished, outcome "no") and `CancelledState` (abandoned), and treats `Completed`, `Rejected` and `Cancelled` as final. Routing and the tracker took only names and `const DataPayload&`, while the team passes `WorkItem*` and `DataPayload*` (`AssignmentRule::assignParticipant(WorkItem*, ...)`, `WorkflowInstance::payload`).
**Change:**
- `ProgressionTracker::reject(item, data)` routes like `complete` (the definition's decision picks the recovery path), but a rejected item doesn't satisfy dependencies on it.
- `ProgressionTracker::cancel(item)` removes Available or waiting work without routing, so a cancelled branch doesn't block the instance from finishing.
- `ProgressionResult::outcome` is "Completed" or "Rejected", using the same strings as `WorkItemState::getStateName()`.
- `ProgressionResult::reopened` lists items a retry loop reaches again. Their old `WorkItem` is in a final state, so the instance creates a fresh one.
- `RoutingRule::determineNextWork`, `complete`, `reject` and `cancel` also accept `const WorkflowComponent*` (so a `WorkItem*`) and `const DataPayload*`. A null item throws `std::invalid_argument`, like `WorkItem::assignTo`. A null payload means "no data", because `WorkflowInstance` allows a null initial payload.
- Style matches the team: `override`, `= default` destructors, `nullptr`.

**Why:** The tracker and the State pattern now describe the same lifecycle, and the spec's exception path (rejection, recovery) runs through the engine. No other member's files were changed.
**Diagrams:** Strategy pattern class diagram.
**Code:** `code/progression/ProgressionTracker.*`, `code/routing/RoutingRule.h`, `SequentialRouting.h`, `DataBasedRouting.h`; `tests/envy/test_integration.cpp`.

