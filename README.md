# TIVIDY

COS 214 final project: **Se7en**, a configurable workflow engine in C++11, with two
industries built on it (**Arkham Express** parcel logistics and **Dune Spice Harvest**)
and an interactive terminal interface styled as a subway map.

## Contents

- [Quick start](#quick-start)
- [Building](#building)
- [Using the interface](#using-the-interface)
- [Project layout](#project-layout)
- [Design patterns](#design-patterns)
- [Testing](#testing)
- [Docker](#docker)
- [Documentation (Doxygen)](#documentation-doxygen)

## Quick start

```bash
sudo apt install build-essential cmake libncurses-dev
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/tividy
```

No compiler on your machine? Use [Docker](#docker) instead.

## Building

Requires CMake 3.12+, a C++11 compiler and `libncursesw` (wide-character ncurses).

| Option | Effect |
|---|---|
| `-DTIVIDY_SANITIZE=ON` | AddressSanitizer + UBSan build |
| `-DTIVIDY_WITH_CURSES=OFF` | Build without ncurses (only `--plain` mode) |

CMake picks up every `.cpp` under `code/` and every `tests/*/test_*.cpp` automatically,
so adding a file never needs a build-file change. Everything compiles with
`-Wall -Wextra -pedantic`.

## Using the interface

```bash
./build/tividy            # full-screen TUI
./build/tividy --plain    # line-based mode: same commands, no ncurses
./build/tividy --help     # all options (--echo for scripted input, --log FILE)
```

- **Terminal:** at least **104 × 34**; smaller windows show a resize message.
- **Font:** use a font with box-drawing glyphs (Cascadia Mono in Windows Terminal) and
  `TERM=xterm-256color` for the full palette.
- **Lines:** Arkham Express is line 1 (blue), Dune is line 2 (orange). Work items are
  stations: `●` done, `◉` current, `○` upcoming; the exception route branches off in red.
- **Keys:** `?` shows the keys for any screen. `S C R E A X` start, complete, reject,
  escalate, assign and cancel the selected work item (Reject and Cancel ask to confirm).
  `:` opens the command bar, which accepts the same commands as `--plain`.
- The terminal is restored on exit, Ctrl+C and crashes. If it ever looks wrong, type
  `reset` and press Enter.

## Project layout

```
code/
  core/          WorkflowEngine facade and exceptions
  state/         work-item lifecycle (State)
  routing/       sequential, parallel and data-driven routing (Strategy)
  progression/   dependencies, joins, rejection and cancellation handling
  builder/       workflow definitions (Builder / Director)
  workflow/      stages, work items (Composite), instances (Prototype)
  scenarios/     Arkham Express and Dune industries (Abstract Factory)
  integration/   external warehouse service (Adapter)
  proxy/         role-based access control (Proxy)
  observer/      notifications and monitoring (Observer)
  history/       per-instance event history
  command/       undoable actions (Command)
  decorator/     audit, deadline, retry and validation on tasks (Decorator)
  iterator/      safe traversal of work and history (Iterator)
  visitor/       progress reports (Visitor)
  approval/      approval chain (Chain of Responsibility)
  assignment/    role- and workload-based assignment
  participants/  people, roles and workload
  interface/     TUI, plain mode, EngineGateway bridge to the engine
  app/           main.cpp
tests/           doctest suites + end-to-end demo scripts
docs/            diagrams, research, Visual Paradigm models
```

## Design patterns

State, Strategy, Composite, Builder, Prototype, Abstract Factory, Facade, Adapter, Proxy, Observer,
Command, Decorator, Iterator, Visitor and Chain of Responsibility, each in its own folder
under `code/` (see the layout above). Class diagrams are in `docs/diagrams` and `docs/vpp`.

## Testing

Tests use [doctest](https://github.com/doctest/doctest) through CTest at three levels:
unit (one class or pattern), integration (patterns working together through the engine)
and end to end (the real program driven by a script).

| Suite | Cases | Assertions | Result |
|---|---|---|---|
| `tividy_tests` (engine: unit + integration) | 187 | 831 | All pass |
| `gateway_tests` (UI ↔ engine bridge) | 9 | 93 | All pass |
| `lust_tests` (interface, on a fake engine) | 46 | 15,022 | All pass |
| `demo_arkham_demo`, `demo_dune_demo` | 2 scripts | — | Both pass |

```bash
ctest --test-dir build --output-on-failure   # everything, including the demo scripts
./build/tividy_tests                         # engine only
./build/tests/lust/lust_tests                # interface only

# with sanitizers
cmake -S . -B build-asan -DTIVIDY_SANITIZE=ON && cmake --build build-asan && ctest --test-dir build-asan
```

The demo scripts (`tests/scripts/*_demo.txt`) are piped into `tividy --plain --echo`;
each passes if the program exits cleanly and prints every line in the matching `.expect`.

`./build/tests/lust/lust_scenario_tests` is an extra stress suite, not part of CTest
(92 cases, including a random walk that checks undo restores state exactly). 75 pass; the
17 failures are deliberate and document known engine limitations.

## Docker

Build and run with only Docker installed: no compiler, CMake or ncurses needed. The image
runs the full test suite during `docker build`, so a failing test means no image.

```bash
# build (from the repo root; note the final ".")
docker build -t tividy .

# run
docker run -it --rm tividy                                   # full-screen TUI (q to quit)
docker run -it --rm tividy ./build/tividy --plain            # typed-command mode
docker run -it --rm tividy ./build/tividy --help             # all options

# tests
docker run --rm tividy ctest --test-dir build --output-on-failure
docker run --rm tividy ./build/tividy_tests                  # engine tests only
docker run --rm tividy ./build/tests/lust/lust_tests         # interface tests only

# demo scripts
docker run -i --rm tividy sh -c "./build/tividy --plain --echo < tests/scripts/arkham_demo.txt"
docker run -i --rm tividy sh -c "./build/tividy --plain --echo < tests/scripts/dune_demo.txt"

# open a shell inside the container
docker run -it --rm tividy bash

# clean up
docker image ls              # list images
docker rmi tividy            # delete the TIVIDY image
docker system prune          # delete stopped containers and unused data
```

- `-it` connects your keyboard and terminal (needed for the TUI and `--plain`); `--rm`
  deletes the container when it exits.
- Rebuild after every `git pull`: the image is a snapshot of the code when it was built.
- Run these in a real terminal (WSL, Windows Terminal, VS Code), not Docker Desktop's
  Run button or Logs tab.

Installation and troubleshooting: [docker.md](docker.md).

## Documentation (Doxygen)

Headers under `code/` carry Doxygen comments (`@brief`, `@param`, `@return`). The
`Doxyfile` in the repo root is already configured (input `code/`, HTML only, UML-style
class and collaboration diagrams).

```bash
sudo apt install doxygen graphviz      # once; graphviz draws the diagrams
doxygen Doxyfile                       # writes docs/doxygen/html
explorer.exe "$(wslpath -w docs/doxygen/html/index.html)"   # WSL: open in Windows browser
xdg-open docs/doxygen/html/index.html                       # Linux
```

Start at **Classes → Class List** or **Class Hierarchy**. The generated `docs/doxygen/`
folder is git-ignored; regenerate it after changing headers.
