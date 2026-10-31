# Test stand-ins (tests only, never part of the engine)

On `integration-Code` the team's `WorkItem.cpp` cannot compile yet: the six State headers in
`code/state/` (`CreatedState.h`, `AvailableState.h`, `AssignedState.h`, `StartedState.h`,
`CompletedState.h`, `EscalatedState.h`) are empty, and `Event.h` (Observer) does not exist.
There is also no `Stage` class (Composite) yet.

So that Envy's Visitor and Iterator can be tested against the real `WorkItem.h` interface now,
these files stand in for the missing parts, **in the test program only**:

| File | Stands in for | Remove when |
|---|---|---|
| `WorkItemStandIn.cpp` | `code/workflow/WorkItem.cpp` (same class, same header, simplified lifecycle with no legality checks) | Pride's State headers and Sloth's `Event.h` exist and `WorkItem.cpp` compiles: link the real file instead |
| `StandInState.h` | the concrete `WorkItemState` classes | same as above |
| `StageStandIn.h` | Pride's `Stage` (Composite), following the Prac 6 design: `accept()` calls `visitStage(this)` then passes the visitor to each child | Pride adds `Stage.h` / `Stage.cpp` |

`WorkItem::accept` in the stand-in is the one line Pride needs to add to `WorkItem.cpp`:
`void WorkItem::accept(WorkflowVisitor* visitor) { visitor->visitWorkItem(this); }`
