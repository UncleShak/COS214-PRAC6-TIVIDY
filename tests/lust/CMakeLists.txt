# Lust's interface tests: a UI-only harness that drives the interface against FakeEngine,
# so the controller and line interpreter are tested without the engine.
# FLAGGED: these tests never exercise the real engine; tests/gateway does that.
#
#   cmake -S tests/lust -B build-lust
#   cmake --build build-lust
#   ctest --test-dir build-lust --output-on-failure     (or ./build-lust/lust_tests)

cmake_minimum_required(VERSION 3.10)
project(lust_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${REPO_ROOT}/code/interface ${CMAKE_CURRENT_BINARY_DIR}/interface)

# Fake engine: tests only, never part of the shipped program.
add_library(lust_fake STATIC fake/FakeEngine.cpp)
target_include_directories(lust_fake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fake)
target_link_libraries(lust_fake PUBLIC tividy_ui_core)
target_compile_options(lust_fake PRIVATE -Wall -Wextra)

add_executable(lust_tests
    test_main.cpp
    test_validation.cpp
    test_controller.cpp
    test_approvals.cpp
    test_interpreter.cpp
    test_layout.cpp
    test_route_line.cpp
)
target_include_directories(lust_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${REPO_ROOT}/code/interface)
target_link_libraries(lust_tests lust_fake tividy_ui_core)

enable_testing()
add_test(NAME lust_tests COMMAND lust_tests)


# Scenario / robustness tests against the REAL engine (only inside the root build, where the engine exists).
# FLAGGED: failing tests here document engine bugs on purpose.
if(TARGET tividy_engine)
  add_executable(lust_scenario_tests
      ${REPO_ROOT}/tests/envy/test_main.cpp
      test_scenarios.cpp
      test_engine_robustness.cpp
  )
  target_include_directories(lust_scenario_tests PRIVATE ${REPO_ROOT}/tests/envy ${REPO_ROOT}/code ${REPO_ROOT}/code/interface)
  target_link_libraries(lust_scenario_tests PRIVATE tividy_engine)
  if(TARGET tividy_real_engine)
    target_sources(lust_scenario_tests PRIVATE test_gateway_real.cpp)
    target_link_libraries(lust_scenario_tests PRIVATE tividy_real_engine)
  endif()
  tividy_configure(lust_scenario_tests)
endif()
