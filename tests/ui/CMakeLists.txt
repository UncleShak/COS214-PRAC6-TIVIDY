# Interface tests: a UI-only harness that drives the interface against FakeEngine,
# so the controller and line interpreter are tested without the engine.
# These tests never exercise the real engine; tests/gateway does that.
#
#   cmake -S tests/ui -B build-ui
#   cmake --build build-ui
#   ctest --test-dir build-ui --output-on-failure     (or ./build-ui/ui_tests)

cmake_minimum_required(VERSION 3.10)
project(ui_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${REPO_ROOT}/src/ui ${CMAKE_CURRENT_BINARY_DIR}/interface)

# Fake engine: tests only, never part of the shipped program.
add_library(ui_fake STATIC fake/FakeEngine.cpp)
target_include_directories(ui_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fake)
target_link_libraries(ui_fake PUBLIC tividy_ui_core)
target_compile_options(ui_fake PRIVATE -Wall -Wextra)

add_executable(ui_tests
    test_main.cpp
    test_validation.cpp
    test_controller.cpp
    test_approvals.cpp
    test_interpreter.cpp
    test_layout.cpp
    test_route_line.cpp
)
target_include_directories(ui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${REPO_ROOT}/tests/support ${REPO_ROOT}/src/ui)
target_link_libraries(ui_tests ui_fake tividy_ui_core)

enable_testing()
add_test(NAME ui_tests COMMAND ui_tests)

