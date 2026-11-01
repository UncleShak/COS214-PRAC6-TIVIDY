# TIVIDY in a container: builds the engine, the TUI and every test, then runs the TUI.
#
#   docker build -t tividy .
#   docker run -it --rm tividy                                          # full-screen TUI
#   docker run -it --rm tividy ./build/tividy --plain                   # typed commands
#   docker run --rm tividy ctest --test-dir build --output-on-failure   # all tests

FROM ubuntu:24.04

# Compiler, CMake and ncurses (the TUI's terminal library). No prompts during install.
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake libncurses-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /tividy
COPY . .

# Build everything. The build fails here if the code does not compile.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j"$(nproc)"

# Run the tests while building the image, so a broken commit never makes an image.
RUN ctest --test-dir build --output-on-failure

# A terminal type the TUI understands, and UTF-8 for its box-drawing characters.
ENV TERM=xterm-256color \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

CMD ["./build/tividy"]