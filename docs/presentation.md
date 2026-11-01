# TIVIDY: Testing, Docker and Doxygen

---

## 1. Testing

### How we test
- **Framework:** [doctest](https://github.com/doctest/doctest), a single-header C++ unit-testing library, with one shared `main()` in `tests/unit/test_main.cpp`.
- **Build system:** CMake and CTest. CMake finds every `tests/*/test_*.cpp` automatically, so adding a test needs no build-file changes.
- **Strict compiling:** `-Wall -Wextra -pedantic`, plus an optional AddressSanitizer/UBSan build (`-DTIVIDY_SANITIZE=ON`) that catches memory errors and undefined behaviour.
- **Three levels:** unit tests (one pattern or class), integration tests (patterns working together through the engine), and end-to-end tests (the real program driven by a script).

### Results at a glance

| Test suite | Test cases | Assertions | Result |
|---|---|---|---|
| `tividy_tests` (engine: unit + integration) | 187 | 831 | All pass |
| `gateway_tests` (UI ↔ real engine bridge) | 9 | 93 | All pass |
| `ui_tests` (interface, on a fake engine) | 46 | 15,022 | All pass |
| `demo_arkham_demo`, `demo_dune_demo` (end to end) | 2 scripts | — | Both pass |
| **Total in CTest** | **244 + 2 scripts** | **15,946** | **100% pass** |

### What each group tests

**Lifecycle / State pattern** (`tests/unit`, 7 cases)
- Valid transitions move a work item along its normal path (Available → Assigned → Started → Completed).
- Invalid transitions throw `logic_error` and leave the state unchanged.
- Rejected, Cancelled and Completed are terminal; an escalated item must be reassigned before work can continue.

**Routing, dependencies and progression** (`tests/unit`, 81 cases)
- **Routing (Strategy):** sequential paths, parallel splits, data-based decisions, default branches when data is missing, and refusing invalid paths.
- **Dependencies:** an item stays blocked until all its prerequisites are done; self-dependencies and cycles are refused.
- **Progression:** a whole workflow runs from start to finish; the exception path (an invalid order goes to cancellation); parallel branches join correctly.
- **Rejection and cancellation:** a rejected step opens a recovery path instead of stalling; cancelling one step cascades correctly, so nothing waits forever.
- **Instance outcomes:** success, cancelled, returned, abandoned, evacuated and stopped, and undo rolls the outcome back.
- **Iterator:** walks available work and history safely, even when work completes during the walk.
- **Visitor:** double dispatch through work items, stages and decorators; progress reports.

**Builder, Adapter, assignment and participants** (`tests/unit`, 40 cases)
- **Builder/Director:** builds complete workflow definitions; each built result is independent of the others.
- **Adapter:** translates our requests for the external warehouse service; handles refusals, duplicate requests, an offline service and invalid input without corrupting data.
- **Assignment (Strategy):** role-based and workload-based selection; fails safely when nobody is eligible.
- **Participants:** roles, ids and workload tracking.

**Engine integration** (`tests/integration`, 45 cases)
- **Facade:** Arkham Express (parcel delivery) and Dune Spice Harvest run from start to finish.
- **Prototype:** two instances never share work items.
- **Abstract Factory:** both industries install side by side.
- **Proxy:** access control refuses actions for the wrong role before the action reaches the State pattern.
- Industry scenarios: denied permits, worm-sign interrupts, warehouse outages.
- **Regression tests for bugs we fixed:** returned parcels give their stock back, only the assigned person can reject work, and the clock refuses to overflow.

**Observer and history** (`tests/unit`, 14 cases)
- Event sources attach, detach and notify listeners; the notification and monitoring observers; the instance history records every event.

**Gateway** (`tests/gateway`, 9 cases)
- The bridge the UI uses to talk to the real engine: route maps, industries, instance data, views of work items, and readable errors that change nothing.

**User interface** (`tests/ui`, 46 cases on a fake engine)
- **Controller:** loading industries, starting instances, the assign/start/complete flow.
- **Approvals:** only the right role can approve; a rejection needs a reason; deadlines escalate.
- **Command interpreter:** typed commands, error messages, rejections with reasons.
- **Input validation:** required, numeric, length and yes/no fields.
- **Layout and route line:** the screen layout at every size from the minimum up; the subway-style route map.

**End-to-end demo scripts** (`tests/e2e`)
- `arkham_demo.txt` and `dune_demo.txt` are fed into the real program (`tividy --plain --echo`). Each passes only if the program exits cleanly and prints every line listed in the matching `.expect` file.

### Extra stress suite (not part of CTest)
- `stress_tests` holds 92 robustness cases against the real engine, including a random walk of thousands of actions that checks undo restores the state exactly.
- **75 of its 92 cases pass.** The 17 that fail are flagged in the code as documenting known engine limitations on purpose, for example separation of duties on approvals, refused actions writing to the audit log, and installing an industry all-or-nothing.

### How to run the tests
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure   # every suite + both demo scripts

./build/tividy_tests                         # engine tests on their own
./build/tests/ui/ui_tests                # interface tests
./build/stress_tests       # stress suite (known failures)
```
With sanitizers:
```bash
cmake -S . -B build-asan -DTIVIDY_SANITIZE=ON && cmake --build build-asan && ctest --test-dir build-asan
```

---

## 2. Docker

### Why
- Anyone can build and run TIVIDY with **only Docker installed**: no compiler, CMake or ncurses on their own machine.
- Everyone gets the same environment, so no more "works on my machine".
- **The tests run while the image builds.** If code doesn't compile or a test fails, `docker build` fails, so a broken commit never produces an image.

### What the Dockerfile does
1. Starts from a clean **Ubuntu 24.04**.
2. Installs `build-essential` (g++), `cmake` and `libncurses-dev` (the TUI's terminal library).
3. Copies the repo in. `.dockerignore` leaves out local `build/` folders and `.git`, so every build starts fresh.
4. Builds everything in **Release** mode with CMake.
5. Runs **`ctest`**, so any failing test stops the build.
6. Sets `TERM` and UTF-8 for the TUI's box-drawing characters, then starts `./build/tividy` by default.

### Commands

| What | Command |
|---|---|
| Build the image | `docker build -t tividy .` |
| Full-screen TUI | `docker run -it --rm tividy` |
| Typed-command mode | `docker run -it --rm tividy ./build/tividy --plain` |
| Run all tests | `docker run --rm tividy ctest --test-dir build --output-on-failure` |
| Dune demo script | `docker run -i --rm tividy sh -c "./build/tividy --plain --echo < tests/e2e/dune_demo.txt"` |

- `-it` connects your keyboard and terminal (needed for the TUI); `--rm` deletes the container when you exit.
- Full setup and troubleshooting: `docker.md` in the repo root.

---

## 3. Doxygen

### What it is
- Doxygen generates browsable HTML documentation (class lists, member descriptions, inheritance and collaboration diagrams) from comments in the source code.

### How we used it
- Classes are documented with Doxygen comments in their headers (`/** @brief ... */`, `@param`, `@return`).
- **78 of our 95 headers** under `src/` carry Doxygen `@brief` comments.
- Doxygen generates HTML pages for every class and pattern participant across `src/`: `engine`, `workflow`, `execution`, `people`, `tasks`, `security`, `monitoring`, `industries` and `ui`.

### How to generate the docs
```bash
sudo apt install doxygen graphviz      # graphviz draws the class diagrams
doxygen -g Doxyfile                    # create a default config, then set:
#   INPUT            = code
#   RECURSIVE        = YES
#   OUTPUT_DIRECTORY = docs/doxygen
#   GENERATE_LATEX   = NO
#   HAVE_DOT         = YES             (class / collaboration graphs)
doxygen Doxyfile
# open docs/doxygen/html/index.html in a browser
```
