# TIVIDY

COS 214 final project: the Se7en workflow engine, with two industries (Arkham Express
logistics and Dune spice harvesting) and an interactive interface.

## Building

Requires CMake 3.10+, a C++11 compiler and `libncursesw` (the wide-character ncurses).
On Ubuntu / WSL:

```bash
sudo apt install build-essential cmake libncurses-dev   # provides libncursesw
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## The interface (TUI)

```bash
./build/tividy            # full-screen interface, styled as a New York subway map
./build/tividy --plain    # line-based interface: same commands, no ncurses
./build/tividy --help     # all options (--echo for scripted --plain input, --log FILE)
```

- **Terminal size:** at least **104 × 34**. Smaller windows show a "make the window bigger"
  message until you resize.
- **Font:** in Windows Terminal use **Cascadia Mono** (the default), so the route glyphs
  (● ◉ ○ ━ ╲) and rounded borders line up. Set `TERM=xterm-256color` for the full palette;
  terminals with fewer colours fall back to basic colours.
- **Dependency:** `libncursesw`. Without it, build with `-DTIVIDY_WITH_CURSES=OFF` and
  only `--plain` is available.
- **`--plain`:** a line-based fallback for scripts, small terminals or screen readers. It
  accepts the same commands as the TUI's `:` command bar.

Each industry is a subway line: Arkham Express is line 1 (blue), Dune is line 2 (orange).
On the dashboard the route box shows an instance's work items as stations: `●` done,
`◉` current, `○` upcoming, and the exception route drops away in red. Press `?` on any
screen for its keys; `S C R E A X` act on the selected work item (start, complete, reject,
escalate, assign, cancel), and Reject and Cancel ask for the same key again to confirm.

The terminal is always restored on exit, on Ctrl+C and if the program crashes. If it ever
looks wrong, type `reset` and press Enter.
