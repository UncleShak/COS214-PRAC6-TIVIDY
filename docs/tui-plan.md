# TIVIDY interactive interface: plan (Lust)

Status: **approved 30 Oct**, with these answers:
- Seam as a separate proposal header, and link-time engine selection.
- "Command layer" renamed to "interface controller".
- Lust has its own doctest copy.
- Work branches off `integration-Code`. `main` only holds the website.
- Branch names use `lust-topic`: git can't create `lust/topic` while a `lust` branch exists.
- Conventions are free as long as authorship is clear (`@author` and `[lust]` commits).
Author: Lust. Date: Fri 30 Oct 2026. Demo: Mon 2 Nov 2026.

This plan is based on a read-only scan of branch `lust` as of 30 Oct. Anything marked **(unverified)** was not confirmed in the repo.

---

## 1. Repo findings

### 1.1 What exists today

| Topic | Finding |
|---|---|
| Build | No root `CMakeLists.txt`. The only CMake file is `tests/envy/CMakeLists.txt`: CMake ≥ 3.10, `CMAKE_CXX_STANDARD 11`, `CMAKE_CXX_EXTENSIONS OFF`. It builds a static lib `envy_engine` (data, routing, progression, visitor, iterator) and `envy_tests`. There is no `main()` anywhere. |
| Test framework | **doctest**, single header, vendored twice (`tests/envy/doctest.h`, `tests/greed/doctest.h`). `tests/greed` is not wired into any CMake file. |
| Engine facade | `code/core/WorkflowEngine.h` is pure virtual with **no implementation**. It has three calls: `startInstance(const std::string& definitionName)`, which returns `WorkflowInstance*`; `completeWorkItem(const std::string& instanceId, const std::string& workItemId)`; and `assignWork(const std::string& instanceId)`. It has no list, query or history calls. The diagram and `docs/envy/patterns.md` also list `deployWorkflow` and `getAvailableWork`. |
| Engine-call interface for the UI | **None exists.** The seam must be proposed (§2). |
| State names | These are the strings returned by `WorkItemState::getStateName()` and relied on by the visitors: `Created`, `Available`, `Assigned`, `Started`, `Completed`, `Rejected`, `Cancelled`, `Escalated`. `RejectedState` and `CancelledState` are empty files, so those two names come from the transitions and visitors and are **not confirmed in a class**. |
| Transitions (as coded in the state `.cpp` files) | Created→Available (makeAvailable). Available→Assigned (assign) / Cancelled (cancel) / Escalated (escalate). Assigned→Started (start) / Cancelled. Started→Completed (complete) / Rejected (reject) / Cancelled. Escalated→Assigned (assign) / Cancelled. Completed is final. Every other action throws `std::logic_error`. |
| Tracker status (separate from state) | `ProgressionTracker` keeps its own sets: available, waiting, completed, rejected, cancelled. "Waiting" (prerequisites not met) is **not** a WorkItem state, so the UI must show both. |
| ID types | There is no ID scheme. The facade uses `std::string` IDs. Work items are identified by **name** in routing, the tracker and the dependency graph. `Participant` has `getID()` (a string). Instances have no ID field. |
| Error convention | Exceptions. `code/core/WorkflowException.h`: `WorkflowException : std::logic_error`, with subclasses `ValidationFailed` and `DeadlineExceeded`. States throw plain `std::logic_error`. Routing throws `std::runtime_error` when no branch matches and there is no default. Null or bad arguments throw `std::invalid_argument`, and missing payload keys throw `std::out_of_range`. **(unverified)** whether this is "Wrath's" chosen convention. Nothing in the repo attributes it. |
| Working components (coded and unit-tested) | `ProgressionTracker`, `DependencyGraph`, `SequentialRouting` / `DataBasedRouting` / `RoutingCondition` (with a printable `RoutingDecision::description`), `KeyValuePayload`, the three visitors (they return strings and counts and never print), `AvailableWorkIterator` / `HistoryIterator`, the GoF Commands plus `CommandLog::describeAll()`, `Participant` / `Role`, the two assignment rules, and the four decorators with `ManualClock`. |
| Broken or missing | `WorkflowEngine` impl, `WorkflowInstance.cpp` (destructor defined twice, undeclared `currentStates`, stubbed `start`/`isComplete`), 9 empty files in `code/state/`, `InstanceSnapshot.h`, `Event.h`, `Stage` (only a test stand-in exists), `WorkItem::accept` / `notifyListeners` bodies, `DeadlineDecorator::add/remove` bodies, and a duplicate `code/TaskDecorator.h`. Today `WorkItem` and everything that includes it **does not compile** outside the envy stand-ins. |
| Industries | **No industry code exists.** The spec text names ArkhamExpress (order fulfilment and delivery). The second industry is not named in the repo. `patterns.md` plans Builder (`ScenarioBuilder` per industry, owned by Greed and Gluttony) and maybe an `IndustryFactory`. |
| Data files | None. All test data lives in the test `.cpp` files. |
| Pattern mapping | Lives in `docs/envy/patterns.md`. There is no `docs/patterns.md`. **No GoF pattern is assigned to the interface layer.** The interface (Lust) is named as the **Client** of the Facade (`WorkflowEngine`), Visitor and Iterator, and Lust should consume `createIterator()`, `HistoryIterator` and the three visitors (README §"Lust"). `patterns.md` §5 is a "report section for Lust's PDF". |
| Conventions | Branch `lust/short-topic` and commit `[lust] area: what changed` come from your brief. **(unverified)** in the repo: the git log could not be read during the scan. Doxygen-style comments are already used in envy's code. |

### 1.2 Naming clash to avoid

The repo already has a **GoF Command** layer (`code/command/`). To avoid confusing markers, the brief's "command layer" will be called the **interface controller** (`InterfaceController`) everywhere in code and docs. It may internally run the team's GoF commands through `CommandLog`, but only via the engine. It never creates them itself.

### 1.3 Gaps and owners

| Gap | Owner (per `docs/envy`) | Blocks me? |
|---|---|---|
| `WorkflowEngine` implementation, plus `getAvailableWork`, a history query and instance IDs | Pride | **Yes**: the real-engine milestone |
| State headers, `RejectedState`, `CancelledState`, `Stage`, `WorkItem::accept` | Pride | Yes: anything real touching `WorkItem` won't compile |
| `WorkflowInstance` fixes, `InstanceSnapshot.h`, `Event.h`, history `entries()` | Sloth / Pride | Yes for history; no for the fake |
| Both industry definitions (builders, definition names, participants, initial data fields) | Greed / Gluttony | **Yes**: "both industries" requirement |
| Error convention sign-off | Wrath (per your brief) | No: I code to the exceptions that exist today |
| Root `CMakeLists.txt` (who owns it) | Unassigned | Partly: my targets can stand alone |
| Second creational pattern | Group leader | No |

---

## 2. The seam

There is no existing engine-call interface, so this is a **proposal that needs Pride's approval** (PR plus decisions-log entry). It uses only C++11, plain value types (no engine pointers cross the seam) and string IDs, and it follows the existing exception convention.

Proposed file: `code/interface/EngineGateway.h`. It's a separate header so `WorkflowEngine.h` is not touched. If Pride prefers to extend `WorkflowEngine` itself, the method list maps one-to-one.

```cpp
/// @file EngineGateway.h
/// @brief Everything the interactive interface needs from the engine.
///        PROPOSAL, needs Pride's approval before merge.
#ifndef TIVIDY_ENGINE_GATEWAY_H
#define TIVIDY_ENGINE_GATEWAY_H

#include <map>
#include <string>
#include <vector>

namespace tividy {
namespace ui {

/// One industry and the workflow definitions it offers.
struct IndustryInfo {
    std::string id;                       ///< e.g. "arkham"
    std::string displayName;              ///< e.g. "ArkhamExpress"
    std::vector<std::string> definitions; ///< definition names startable in this industry
};

/// A field the user must or may supply when starting an instance.
struct DataField {
    std::string key;          ///< KeyValuePayload key
    std::string label;
    bool numeric;             ///< validated with the same rule as KeyValuePayload::getNumber
    bool required;
    std::string defaultValue;
};

struct ParticipantView {
    std::string id;
    std::string name;
    std::vector<std::string> roles;
    int workload;
};

/// Snapshot of one work item, safe to keep after the engine changes.
struct WorkItemView {
    std::string id;            ///< currently the item name (engine has no other ID)
    std::string name;
    std::string state;         ///< exact getStateName() string: "Created", "Available", ...
    std::string trackerStatus; ///< "available", "waiting", "completed", "rejected", "cancelled", or ""
    std::string requiredRole;
    std::string assigneeName;  ///< "" if unassigned
    std::vector<std::string> waitingOn; ///< unmet prerequisites
};

struct InstanceView {
    std::string id;
    std::string industryId;
    std::string definitionName;
    std::string definitionVersion;
    bool complete;
    bool stalled;
    std::map<std::string, std::string> data; ///< KeyValuePayload::entries()
};

/// What the user can do to a work item. Mirrors the team's GoF Commands.
enum class WorkAction { Assign, Start, Complete, Reject, Cancel, Escalate };

struct ActionRequest {
    std::string instanceId;
    std::string workItemId;
    WorkAction action;
    std::string actorId;     ///< participant performing the action
    std::string assigneeId;  ///< Assign only; "" = let the AssignmentRule choose
    std::string reason;      ///< Reject only
};

/// What happened, enough to show the state change and routing.
struct ActionOutcome {
    std::string previousState;
    std::string newState;
    std::string commandDescription;          ///< Command::describe()
    std::string routingDescription;          ///< RoutingDecision::description, "" if no routing ran
    std::vector<std::string> madeAvailable;  ///< ProgressionResult
    std::vector<std::string> nowWaiting;
    std::vector<std::string> reopened;
    bool instanceComplete;
};

/// @brief Abstract engine seam used by the interface. Every call either
///        succeeds or throws std::exception (WorkflowException for illegal
///        workflow operations, std::invalid_argument for unknown IDs or bad
///        input). A failed call must leave engine state unchanged.
class EngineGateway {
public:
    virtual ~EngineGateway() {}

    virtual std::vector<IndustryInfo> listIndustries() const = 0;
    virtual std::vector<DataField> startFields(const std::string& industryId,
                                               const std::string& definitionName) const = 0;
    /// @return the new instance's ID
    virtual std::string startInstance(const std::string& industryId,
                                      const std::string& definitionName,
                                      const std::map<std::string, std::string>& initialData) = 0;

    virtual std::vector<InstanceView> listInstances(const std::string& industryId) const = 0;
    virtual InstanceView getInstance(const std::string& instanceId) const = 0;

    virtual std::vector<WorkItemView> listWorkItems(const std::string& instanceId) const = 0;
    virtual std::vector<WorkItemView> listAvailableWork(const std::string& instanceId) const = 0;
    virtual WorkItemView getWorkItem(const std::string& instanceId,
                                     const std::string& workItemId) const = 0;

    virtual std::vector<ParticipantView> listParticipants(const std::string& industryId) const = 0;

    virtual ActionOutcome act(const ActionRequest& request) = 0;

    /// History lines, oldest first (CommandLog::describeAll + routing + state changes).
    virtual std::vector<std::string> history(const std::string& instanceId) const = 0;

    /// Visitor outputs.
    virtual std::string progressSummary(const std::string& instanceId) const = 0;
    virtual std::vector<std::string> auditReport(const std::string& instanceId) const = 0;
    virtual std::vector<std::string> overdueWork(const std::string& instanceId) const = 0;
};

} // namespace ui
} // namespace tividy
#endif
```

Notes for Pride:
- `act` takes one request rather than six methods, so new actions don't change the vtable. Under the hood it maps 1:1 to `AssignCommand`…`EscalateCommand` run through `CommandLog`.
- Value structs mean the interface never holds `WorkItem*`, so a later engine refactor can't leave dangling pointers in the UI.
- Optional, demo only: `virtual void advanceClock(int units)`, which would show `DeadlineDecorator` causing escalation on stage. Include it only if Pride wants deadlines in the demo.
- **Pattern note (not a claim):** the real implementation of this seam would sit in front of `WorkflowEngine` and translate types, which is close to a GoF Adapter. The team design has no interface-layer pattern, so I will **not** label it as one in the report unless you and the team agree.

---

## 3. File plan

All new files are mine. No teammate file is edited.

| File | Layer | Purpose |
|---|---|---|
| `code/interface/EngineGateway.h` | Seam | §2. Needs a PR approved by Pride. |
| `code/interface/ViewModels.h` | Controller | Screen-agnostic results (`ScreenModel`, `StatusMessage {Info, Success, Error}`, list rows). |
| `code/interface/InterfaceController.h/.cpp` | Controller | Holds `EngineGateway&` and UI session state (current industry, instance, selection). One method per user action (§5). Validates input, catches engine exceptions and turns them into `StatusMessage`s. No ncurses, no `cin`/`cout`. |
| `code/interface/InputValidation.h/.cpp` | Controller | Trims and checks free text: required, numeric, length, menu index. Pure functions, easy to test. |
| `code/interface/Log.h/.cpp` | Shared | Appends debug lines to a file (`tividy.log`, or `--log PATH`). This is the only debug output while ncurses runs. |
| `code/interface/plain/PlainFrontEnd.h/.cpp` | Plain | Numbered menus over `std::getline`/`std::cout`. EOF on stdin means quit. Renders the same `ScreenModel`s. |
| `code/interface/tui/CursesSession.h/.cpp` | ncurses | RAII guard. The constructor calls `setlocale` (before), `initscr`, `cbreak`, `noecho`, `keypad(stdscr, TRUE)`, `curs_set(0)`, `set_escdelay(25)`, `start_color`, `use_default_colors`. The destructor calls `endwin`. Copy constructor and assignment are private (C++11 `= delete`). Installs the `SIGINT` handler. |
| `code/interface/tui/Theme.h/.cpp` | ncurses | Colour pairs: bone 230, gold 179, muted 101, red 160. If `COLORS < 256`, falls back to the 8 basic colours (white, yellow, white-dim, red). Maps a state name to a colour pair. |
| `code/interface/tui/Layout.h/.cpp` | ncurses | Creates and resizes four windows (header, list, detail, status) and does the minimum-size check. Draws with `wnoutrefresh` and one `doupdate()`. |
| `code/interface/tui/Widgets.h/.cpp` | ncurses | Scrollable list with a selection, a modal menu, a single-line text prompt (Esc cancels), and a confirm dialog. |
| `code/interface/tui/TuiFrontEnd.h/.cpp` | ncurses | Event loop. Turns keys into controller calls and renders `ScreenModel`s. Handles `KEY_RESIZE`. |
| `code/interface/main.cpp` | Entry | Parses `--plain`, `--log`, `--help`. Gets the engine from `makeEngine()`, picks a front end, and catches everything **outside** the `CursesSession` scope. |
| `code/interface/EngineFactory.h` | Seam | Declares `std::unique_ptr<EngineGateway> makeEngine();` The definition is chosen at link time (see below). |
| `code/interface/RealEngineFactory.cpp` | Seam | Defines `makeEngine()` returning the real gateway. It is written when the engine lands (§7). |
| `code/interface/CMakeLists.txt` | Build | `tividy_ui` static lib (controller, plain, tui, log), `tividy` executable (lib + `RealEngineFactory.cpp` + team engine libs). |
| `tests/lust/CMakeLists.txt` | Build | `lust_tests` (doctest) and `tividy_dev` (the UI linked to the fake, for development only). |
| `tests/lust/fake/FakeEngine.h/.cpp` | Test | Fake `EngineGateway` (§6.1). |
| `tests/lust/fake/FakeEngineFactory.cpp` | Test | `makeEngine()` returning `FakeEngine`. |
| `tests/lust/test_main.cpp`, `test_controller.cpp`, `test_validation.cpp`, `test_gateway_contract.cpp` | Test | §6. Lust vendors its own `tests/lust/doctest.h` (2.4.11, same version as envy), so it doesn't depend on another member's folder. |

**Why link-time selection instead of "one line in main":** `main.cpp` never mentions the fake, and the shipped `tividy` target cannot contain it, because `FakeEngine.cpp` is only compiled in `tests/lust`. Swapping means writing `RealEngineFactory.cpp` and changing nothing in `main`. If you'd rather have the literal one-line swap, `main` would `#include` a factory header and the change is the include. Both work.

**CMake changes:**
- New `code/interface/CMakeLists.txt` and `tests/lust/CMakeLists.txt`, each standalone like envy's (`cmake -S tests/lust -B build-lust`).
- ncurses: `set(CURSES_NEED_WIDE TRUE)`, `find_package(Curses REQUIRED)`, `target_include_directories(... ${CURSES_INCLUDE_DIRS})`, `target_link_libraries(... ${CURSES_LIBRARIES})`. Only the `tui/` sources need it. Option `TIVIDY_WITH_CURSES` (default ON). When it is OFF, the build produces a plain-only binary, which is also a fallback if the demo laptop lacks the dev package.
- Proposed (needs team agreement): a root `CMakeLists.txt` that `add_subdirectory`s each module and both test folders, so the README has one build command.

---

## 4. Screens (ncurses)

Minimum size 100×30. The header is 1 line, the status bar is 2 lines, and the body is split about 40/60. Mockups use ASCII. The real UI uses ACS box characters.

**Global keys:** `↑/↓` move, `PgUp/PgDn` page, `Enter` select or open the action menu, `Tab` switch pane, `Esc`/`b` back, `?` help, `q` quit (with confirmation), `Ctrl+C` quits immediately (the terminal is restored).

### 4.1 Industry picker (start screen)
```
 TIVIDY  Se7en workflow engine                                              [? help]  [q quit]
+-- INDUSTRY ---------------------------+-- ABOUT -----------------------------------------------+
| > ArkhamExpress                        | Order fulfilment and delivery.                         |
|   <Industry 2>                         | Definitions: Order Fulfilment v1                       |
|                                        | Running instances: 2                                   |
+----------------------------------------+--------------------------------------------------------+
 Enter open industry   q quit
```

### 4.2 Instances (per industry)
```
 TIVIDY > ArkhamExpress                                                      2 instances
+-- INSTANCES ---------------------------+-- INSTANCE -------------------------------------------+
| > ARK-0001  Order Fulfilment v1  Done  | ARK-0002   Order Fulfilment v1                        |
|   ARK-0002  Order Fulfilment v1  4/9   | Progress: 4 work items: 3 Completed, 1 Available (75%)|
|                                        | Data: orderValue=1500  destination=Gotham  urgent=yes |
+----------------------------------------+--------------------------------------------------------+
 Enter open   n new instance   i change industry   b back
```

### 4.3 Start instance (form, opened by `n`)
```
+-- START: Order Fulfilment v1 ------------------------------+
|  Order value (number)*  [1500______]                      |
|  Destination*           [Gotham____]                      |
|  Urgent (yes/no)        [no________]                      |
|                                                            |
|  Enter next field / confirm   Esc cancel                   |
+------------------------------------------------------------+
 ! Order value must be a number                (red, field stays focused)
```
The form only appears when `startFields()` returns fields. If it returns none, the instance starts straight away.

### 4.4 Instance dashboard (main working screen)
```
 TIVIDY > ArkhamExpress > ARK-0002 (Order Fulfilment v1)                      4/9  44%  running
+-- WORK ITEMS ---------------------------+-- DETAIL ----------------------------------------------+
|   Receive Order        Completed        | Validate Order                                         |
| > Validate Order       Started          | State:     Started            (gold)                   |
|   Check Stock          Created  waiting | Role:      Clerk                                       |
|   Approve Delivery     Created  waiting | Assignee:  Thandi                                      |
|   Prepare Package      Created  waiting | Waiting on: -                                          |
|   ...                                   | Allowed:   Complete, Reject, Cancel                    |
|                                         |                                                        |
+-----------------------------------------+--------------------------------------------------------+
 Enter actions  v available only  h history  r reports  n new  Tab pane  b back  ? help
 + Thandi started 'Validate Order'   (Assigned -> Started)
```
Colours: Available and Started in gold, Completed in muted, Rejected/Cancelled/Escalated in red, waiting items in muted with the reason in the detail pane. `v` toggles "all items" and "available only", which uses `listAvailableWork`, the iterator-backed list.

### 4.5 Action menu (modal, opened with `Enter` on a work item)
```
+-- Validate Order (Started) --+
| > Complete                    |
|   Reject...                   |
|   Cancel                      |
|   - Assign   (not allowed)    |   <- muted, explains why if chosen
|   - Start    (not allowed)    |
|   - Escalate (not allowed)    |
+-------------------------------+
 Enter do it   Esc close
```
"Allowed" comes from the transition table **as a hint only**. The engine remains the authority: a disallowed action is still sent if chosen, so the marker sees the engine refuse it. Then follow-ups:
- **Assign** opens a participant picker (name, roles, workload). The first option is "Auto (assignment rule)".
- **Reject** opens a text prompt for a reason (required).
- Actions that need an actor use the selected participant. The current actor is shown in the header and changed with `p`.

### 4.6 Outcome panel (shown in the detail pane right after an action)
```
+-- RESULT ------------------------------------------------+
| Thandi completed 'Validate Order'                        |
| State:    Started -> Completed                           |
| Routing:  Decision: orderValue > 1000 -> Approve Delivery|
| Now available: Approve Delivery, Check Stock             |
| Still waiting: Prepare Package (needs Check Stock)       |
+----------------------------------------------------------+
 any key: back to detail
```
This is the main "see routing and state changes" evidence.

### 4.7 History (`h`)
```
 TIVIDY > ArkhamExpress > ARK-0002 > History                         12 entries  oldest first
+--------------------------------------------------------------------------------------------------+
|  1  Thandi assigned 'Receive Order' to Thandi                                                    |
|  2  Thandi started 'Receive Order'                                                               |
|  3  Thandi completed 'Receive Order'  | Sequence: Receive Order -> Validate Order                |
| ...                                                                                              |
+--------------------------------------------------------------------------------------------------+
 o toggle order   / filter   Esc clear filter   b back
```
Filtering and ordering use `HistoryIterator` semantics (substring filter, OldestFirst/NewestFirst) inside the controller.

### 4.8 Reports (`r`): tabs with `←/→`
```
 [ Progress ]  Audit   Overdue
+--------------------------------------------------------------------------------------------------+
| 9 work items: 4 Completed, 1 Started, 1 Available, 3 Created (44% complete)                      |
|                                                                                                  |
| Completed  ####............  4                                                                   |
| ...                                                                                              |
+--------------------------------------------------------------------------------------------------+
```
Audit shows `name | state | holder | role:` lines. Overdue lists escalated items, or "No overdue work".

### 4.9 Terminal too small
```
 Terminal is 80x24. TIVIDY needs at least 100x30.
 Enlarge the window or run:  tividy --plain
```
This screen redraws on every `KEY_RESIZE`. Selection and state are kept.

### 4.10 Help (`?`)
A modal listing the keys for the current screen.

---

## 5. Command list (interface controller actions)

M1 = start an instance, M2 = inspect available work, M3 = take actions, M4 = see routing and state changes, M5 = view history, M6 = both industries, M7 = invalid input and transitions.

| # | User action | Engine calls | Invalid cases to handle | Req. |
|---|---|---|---|---|
| A1 | List industries | `listIndustries` | Empty list ("no industries registered"); engine throws on load | M6 |
| A2 | Select / switch industry | `listInstances(ind)`, `listParticipants(ind)` | Unknown ID; switching while a prompt is open (cancel the prompt first) | M6 |
| A3 | Start instance | `startFields`, `startInstance` | No definitions; required field empty; non-numeric in a numeric field; whitespace-only; overlong text (cap 200); engine refuses; Esc mid-form (nothing created) | M1, M7 |
| A4 | List / open instance | `listInstances`, `getInstance`, `listWorkItems` | No instances yet; unknown or stale ID | M1 |
| A5 | Show available work | `listAvailableWork` | None available: show "stalled" or "complete" using `InstanceView` | M2 |
| A6 | Inspect work item | `getWorkItem` | Item vanished or renamed after refresh | M2 |
| A7 | Assign | `act{Assign}`, `listParticipants` | No participant has the role (rule returns none); item not Available/Escalated; picker cancelled | M3, M7 |
| A8 | Start | `act{Start}` | Actor is not the assignee (`requireAssignee`); wrong state | M3, M7 |
| A9 | Complete | `act{Complete}` | Not the assignee; wrong state; routing finds no branch and no default (`runtime_error`); `ValidationFailed`; `DeadlineExceeded` | M3, M4, M7 |
| A10 | Reject | `act{Reject}` | Empty reason; wrong state | M3, M7 |
| A11 | Cancel | `act{Cancel}` | Already final; confirmation declined | M3, M7 |
| A12 | Escalate | `act{Escalate}` | Wrong state | M3, M7 |
| A13 | Change actor | `listParticipants` | No participants | M3 |
| A14 | View history | `history` | Empty history; filter matches nothing | M5 |
| A15 | Reports | `progressSummary`, `auditReport`, `overdueWork` | Instance with zero items | M2, M4 |
| A16 | Quit | none | Confirm; Ctrl+C; stdin EOF in plain mode | — |
| A17 | Resize / too small | none | Below 100×30 shows the §4.9 screen; resize during a prompt | — |

Rules for every action:
1. The controller catches `std::exception`. The message goes to the status line in red, and the screen re-fetches from the engine so what is shown always matches the engine's state.
2. The controller never decides whether a transition is legal. It forwards the request and reports what the engine says.
3. Menu input is validated before it reaches the engine (index range, empty, numeric). Engine errors and input errors use different prefixes: "Not allowed:" versus "Invalid input:".

---

## 6. Test plan (doctest, `tests/lust`)

### 6.1 The fake engine
- **One small workflow, two fake industries.** The industries are named `Fake Industry A` and `Fake Industry B`, which are deliberately not real names, so they can't be mistaken for demo data. Both use the same graph with different participants, so industry switching can be tested.
- **Graph:** `Receive → Validate → [decision: amount > 1000 → Approve | default → Pack] → Approve → Pack → Ship`. The failure path is `Validate` rejected, which ends the instance as rejected. A cancel can come from any non-final state.
- It uses the **exact state names and transition table** from §1.1, string IDs (`FAKE-0001`…), the item name as the work item ID, and throws `std::logic_error` / `std::invalid_argument` / `std::runtime_error` exactly as the engine does.
- It lives only in `tests/lust/fake/` and is never linked into `tividy`.

### 6.2 Controller tests (`test_controller.cpp`)
1. Industry list loads, and switching industry changes the participants and instances.
2. Starting with valid data creates an instance whose first item is Available.
3. Starting with a missing required field: status shows an error and **no instance is created**.
4. Starting with non-numeric text in a numeric field is rejected before reaching the engine.
5. Esc or cancel mid-form creates nothing.
6. Available work lists only available items, in order.
7. The happy path Assign → Start → Complete updates the states and the outcome shows `previousState`/`newState`.
8. Complete on the decision item with amount > 1000 gives a routing description naming `Approve`. With amount ≤ 1000 it names `Pack` (both branches).
9. Completing a prerequisite moves the waiting items to available (`madeAvailable`).
10. Start by a non-assignee: error status, and the item stays Assigned.
11. Complete while Available (illegal transition): "Not allowed:", state unchanged.
12. Any action on a Completed item: error, unchanged.
13. Reject with an empty reason is refused locally. With a reason, the state becomes Rejected and history records the reason.
14. Assign when no participant has the role gives a clear error.
15. Routing with no matching branch (fake configured with no default): `runtime_error` becomes an error status.
16. Unknown instance or item ID gives an error with no crash.
17. History is oldest-first and grows by one per successful action. Failed actions add nothing.
18. History filter and order toggle.
19. Reports: the progress summary and percentage match after N completions; overdue is empty, then lists an escalated item.
20. Menu index out of range, empty and non-numeric input in the plain-mode parser.
21. Instance completion: after Ship, `complete == true` and no actions are offered.

### 6.3 Validation tests (`test_validation.cpp`)
Trim, required, numeric (same rule as `KeyValuePayload::getNumber`: `strtod` with the whole string consumed), max length, yes/no parsing.

### 6.4 Gateway contract tests (`test_gateway_contract.cpp`)
A suite written once against `EngineGateway&` covering behaviours every implementation must have: unknown ID throws; a failed `act` leaves state unchanged; state names are in the allowed set; history only grows. It runs against the fake now and against the real gateway once it exists (§7). **This is what catches the fake drifting from the real engine.**

### 6.5 Not unit-tested
The ncurses drawing is not unit-tested. It is covered by a manual checklist in the README: resize, too small, colours, Ctrl+C restores the terminal, every screen reachable.

---

## 7. Integration plan

1. **When:** as soon as Pride's facade compiles and at least one industry builder exists. The target is Saturday midday.
2. **How:** write `RealEngineGateway` (implements `EngineGateway` by calling `WorkflowEngine` and reading results back through the visitors, `ProgressionTracker` and `CommandLog`) and `RealEngineFactory.cpp`. It goes in `code/interface/` if Pride agrees it's an interface concern, otherwise Pride hosts it. `main.cpp` does not change.
3. **If the facade is still incomplete on Saturday:** ask the team whether the real gateway may compose the engine classes that already work (`ProgressionTracker`, `DataBasedRouting`, `DependencyGraph`, `CommandLog`, visitors, `Participant`, assignment rules) using the industries' builders. That is the real engine code, not a separate simulation, but it needs the team's sign-off and Greed/Gluttony's definitions. **I will not hard-code a workflow into the shipped program.**
4. **Checks after the swap:**
   - The contract suite (§6.4) passes against the real gateway.
   - The `lust_tests` controller suite still passes on the fake (it should be unchanged).
   - State names shown match `getStateName()` exactly. No "Waiting" appears as a state.
   - Both industries are listed and startable, and each runs end to end in `tividy` and in `tividy --plain`.
   - An illegal transition on the real engine shows an error and leaves state unchanged.
   - History shows real `Command::describe()` lines and routing descriptions.
   - A clean build from scratch on a fresh WSL clone using only the README.

---

## 8. Milestones

| When | Deliverable | Done when |
|---|---|---|
| **Fri 30 Oct (tonight)** | Branch `lust-tui-skeleton`. `EngineGateway.h` (proposal PR opened to Pride), `ViewModels`, `InterfaceController` skeleton, `FakeEngine`, first 8 controller tests green. `CursesSession`, `Layout` with 4 empty panes, resize handling, too-small screen, `q` quits, Ctrl+C restores the terminal. | `tividy_dev` opens and closes cleanly in Windows Terminal. `lust_tests` passes. |
| **Sat 31 Oct, morning** | All of §4 drawn against the fake. The plain front end reaches feature parity. Tests 9–21 done. | Every action in §5 is reachable in both front ends on the fake. |
| **Sat 31 Oct, afternoon** | `RealEngineGateway` against whatever engine exists. Both industries wired. | Contract suite passes on the real engine. |
| **Sat 31 Oct, evening checkpoint** | **Go/no-go.** | **Go** = in `tividy` (ncurses, real engine), for **both** industries: start, see available work, assign/start/complete, see a routing decision, trigger and see one invalid transition, view history, with no crash or garbled screen. **No-go** (any item fails because of the ncurses layer) = `--plain` becomes the default demo mode; ncurses stays on `lust/tui-ncurses` and is not merged. If the item fails because of the **engine**, it's an engine blocker, and switching to plain doesn't fix it. Escalate to the team that night. |
| **Sun 1 Nov** | Polish (colours, help, empty states). Doxygen pass. README (deps, build, run, test, both industries, manual UI checklist). State diagram. Decisions entries. Full rehearsal in the exact Windows Terminal window and font size used on Monday, with a timed 30-minute script. | Rehearsal runs without touching code. |
| **Mon 2 Nov** | Demo from the submitted code. | — |

---

## 9. Risks and fallbacks

| Risk | Likelihood | Mitigation / fallback |
|---|---|---|
| **The real engine is not runnable by Saturday.** No facade impl exists today, and `WorkItem`/states don't compile. | **High** | Raise it tonight (§10). Propose option §7.3. The UI is ready on the fake, so integration is the only remaining work. Note that `--plain` does **not** solve this: the spec requires the real engine either way. |
| Second industry undefined | High | Ask Greed/Gluttony tonight for its name, definition and participant data. |
| Seam not approved, or Pride changes the signatures | Medium | Value-type seam. Changes are contained in `RealEngineGateway`, and the controller is untouched. |
| ncurses rendering issues in Windows Terminal (wide chars, colours, Esc delay, resize artefacts) | Medium | `setlocale`, `CURSES_NEED_WIDE`, `set_escdelay(25)`, the 8-colour fallback, and a full redraw on `KEY_RESIZE`. Test early tonight in the real terminal. |
| `libncurses-dev` missing on the demo laptop | Low | Install and check it on Sunday. `TIVIDY_WITH_CURSES=OFF` builds a plain-only binary. |
| A crash leaves the terminal broken | Medium | RAII guard, a catch-all outside its scope, and a `SIGINT` handler. `reset` is in the README. |
| Building on `\\wsl.localhost` from Windows tools (permissions or "dubious ownership" git errors were seen during the scan) | Medium | Build and run only from the WSL shell inside the Linux filesystem. Document this in the README. |
| Fake drifts from the real engine | Medium | Contract tests (§6.4). |

**Triggers to switch the demo to `--plain`:**
1. The Saturday-evening checkpoint fails for ncurses-specific reasons.
2. Any crash, hang or garbled screen during Sunday's rehearsal that isn't fixed by Sunday 18:00.
3. The ncurses dev package can't be installed on the demo laptop.

The plain mode is rehearsed on Sunday too, whichever one is chosen.

---

## 10. Questions for teammates (blocking)

| # | Question | Owner | Needed by |
|---|---|---|---|
| Q1 | Will `WorkflowEngine` have a concrete implementation by Sat midday? Will you accept the `EngineGateway` seam (§2), or add the equivalent calls to `WorkflowEngine`? | Pride | Fri night |
| Q2 | What is the instance ID format, and are work-item IDs just names (as in routing and the tracker)? | Pride | Fri night |
| Q3 | Will `RejectedState`/`CancelledState` exist, and are `"Rejected"`/`"Cancelled"` the exact names? Is Rejected final, or does the tracker's reopen loop apply? | Pride | Sat am |
| Q4 | Where will history live (Sloth's `History` or `CommandLog`), and will it include routing and state changes? | Sloth | Sat am |
| Q5 | Which two industries are in the demo? For each: definition names, required start data (keys and types), participants and roles, and where the builder lives. | Greed / Gluttony | **Fri night** |
| Q6 | Is the error convention "exceptions derived from `std::logic_error`" (as in `WorkflowException.h`) final? Should routing's `std::runtime_error` become a `WorkflowException`? | Wrath | Sat am |
| Q7 | Who owns a root `CMakeLists.txt`? May I add one that just `add_subdirectory`s the modules? | Group leader | Sat am |
| Q8 | If the facade isn't ready, may the real gateway compose the existing engine classes directly (§7.3)? | Team / Pride | Sat midday |
| Q9 | Is there a team-wide decisions log and patterns file, or only `docs/envy/`? Where should Lust's entries go? | Group leader | Sun |
| Q10 | Do we want the deadline/escalation demo (`advanceClock`)? | Pride / Gluttony | Sat |
| Q11 | Should the interface-side gateway be named as an Adapter in the report, or stay unlabelled? | Team | Sun |

---

## 11. Docs to update

**README** (Lust owns it):
- Dependencies: g++ with C++11, CMake ≥ 3.10, `libncurses-dev` (wide-char; `libncursesw5-dev` on older Ubuntu). doctest is vendored.
- Build, both the root build (if Q7 is agreed) and the per-module builds.
- Run: `tividy`, `tividy --plain`, `--log`, the minimum terminal size, and the recommended Windows Terminal settings.
- A walk-through of both industries, step by step.
- Tests: the commands for each test folder, plus the manual UI checklist.
- Troubleshooting: `reset`, build inside the WSL filesystem.

**Decisions log** (location per Q9), proposed entries:
- L-01 Interactive interface: ncurses TUI plus a `--plain` fallback over a shared, ncurses-free controller.
- L-02 `EngineGateway` seam: value-type views and string IDs (pending Pride).
- L-03 Fake engine confined to `tests/lust/fake`. The real or fake engine is selected at link time.
- L-04 The UI never decides transition legality; the engine is the authority.
- L-05 256-colour Se7en palette with an 8-colour fallback; minimum 100×30.
- L-06 "Command layer" renamed "interface controller" to avoid a clash with the GoF Command pattern.

**State diagram** (Lust owns it): the WorkItem lifecycle from §1.1, with Created as the initial state and Completed, Rejected and Cancelled as final. Annotate each transition with the action and the GoF Command that triggers it. A note shows the tracker's "waiting" status as separate from state. Q3 must be answered before Rejected/Cancelled are finalised. PlantUML source goes alongside the PNG, as envy does.

**Third-party citations for the report:**
- *GNU ncurses*, Thomas E. Dickey et al., Free Software Foundation. Licence: X11/MIT-style. https://invisible-island.net/ncurses/ . Version: record the output of `ncursesw6-config --version` on the demo laptop.
- *doctest*, Viktor Kirilov, MIT licence, https://github.com/doctest/doctest (already used by the team; cite once in the shared references).
