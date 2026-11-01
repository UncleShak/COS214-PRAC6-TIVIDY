# Running TIVIDY with Docker

Docker builds and runs TIVIDY inside a clean Ubuntu 24.04 container. You don't need CMake, a C++ compiler or ncurses on your own machine; you only need Docker.

The image also **runs every test while it builds**. If someone pushes code that doesn't compile or breaks a test, `docker build` fails and shows the error.

## 1. Install Docker (once)

**Windows (WSL):**
1. Install **Docker Desktop** from docker.com and open it.
2. Go to **Settings → Resources → WSL integration**, switch on your Ubuntu distro, and click **Apply & restart**.
3. Open a **new** Ubuntu terminal and check that `docker --version` prints a version.

**Mac:** install Docker Desktop and open it.

**Linux:** `sudo apt install docker.io`, then `sudo usermod -aG docker $USER`, then log out and back in.

Docker Desktop must be **running in the background** whenever you use Docker.

## 2. Build the image

From the repository root, the folder with `Dockerfile` and `CMakeLists.txt` in it:

```
docker build -t tividy .
```

- **Don't forget the `.` at the end.** It means "build from this folder".
- The first build takes a few minutes. Later builds are much faster, because the Ubuntu and package steps are cached.
- Rebuild after every `git pull` or code change. The image is a snapshot of the code at the time you built it.

The build is fine if it ends with `naming to docker.io/library/tividy:latest`. If a test fails, the build stops at step `[6/6] RUN ctest` and prints the failing test.

## 3. Run it

| What | Command |
|---|---|
| Full-screen TUI | `docker run -it --rm tividy` |
| Typed-command mode | `docker run -it --rm tividy ./build/tividy --plain` |
| The Dune demo script | `docker run -i --rm tividy sh -c "./build/tividy --plain --echo < tests/scripts/dune_demo.txt"` |
| All tests, with results | `docker run --rm tividy ctest --test-dir build --output-on-failure` |

- **`-it` is required for the TUI and `--plain`.** It connects your keyboard and terminal to the program.
- **`--rm`** deletes the container when you quit, so nothing piles up. The image stays.
- Press **`q`** to quit the TUI.

### Where to run it

Run these commands in a real terminal: your **Ubuntu (WSL) terminal**, **Windows Terminal**, or **VS Code's terminal panel**.

**Don't** use Docker Desktop's ▶ **Run** button or its **Logs** tab. That isn't a real terminal: the TUI shows up as jumbled boxes and `(B` symbols, and you can't type into it.

Make the terminal at least **110 × 30** for the full layout, with the menu on the left. The minimum is 80 × 24. Use the **Cascadia Mono** font if boxes or symbols look wrong.

## 4. When something goes wrong

| You see | Fix |
|---|---|
| `'docker buildx build' requires 1 argument` | You left out the `.` at the end of `docker build -t tividy .` |
| `The command 'docker' could not be found in this WSL 2 distro` | Turn on WSL integration (step 1), then open a **new** terminal |
| `Cannot connect to the Docker daemon` | Docker Desktop isn't running. Open it and wait until it says it's running. |
| Docker Desktop crashes with `SIGBUS` / `unexpected error` | Quit it, run `wsl --shutdown` in PowerShell, and reopen it. If it keeps crashing, free up disk space, or use **Reset to factory defaults** (this only wipes Docker's own images), then turn WSL integration back on. |
| `Terminal is 70x20. TIVIDY needs at least 80x24.` | Make the window bigger, or use the `--plain` command |
| The build stops at `[5/6]` with compiler errors | The code on your branch doesn't compile. Check `git log`, and tell whoever pushed last. |
| The build stops at `[6/6]` with a failing test | A test is broken on your branch; the output names it |
| Old behaviour after pulling new code | Rebuild: `docker build -t tividy .` |
| Building from PowerShell is very slow | Build from the **Ubuntu (WSL) terminal** instead. PowerShell reads WSL files over a slow network path. |

## 5. What the Dockerfile does

1. Starts from a clean **Ubuntu 24.04**.
2. Installs **build-essential** (g++), **cmake** and **libncurses-dev**, which the TUI needs.
3. Copies the repo in. `.dockerignore` leaves out your local `build/` folders and `.git`, so the image always builds fresh.
4. Builds everything in Release mode: the same `cmake -S . -B build` and `cmake --build build` you run locally.
5. Runs **all the tests** (`ctest`). A failure stops the build.
6. Sets the terminal and UTF-8 settings the TUI needs, and starts `./build/tividy` by default.

## 6. Cleaning up (optional)

```
docker image ls              # list images
docker rmi tividy            # delete the TIVIDY image
docker system prune          # delete stopped containers and unused data
```