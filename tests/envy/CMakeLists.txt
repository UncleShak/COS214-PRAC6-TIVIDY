# Envy's part of TIVIDY: routing, decisions, dependencies and progression.
# Builds Envy's code against the team's abstract DataPayload (code/DataPayload.h)
# and WorkflowComponent (code/workflow/WorkflowComponent.h)
# and runs the doctest suite.
#
#   cmake -S tests/envy -B build-envy
#   cmake --build build-envy
#   cd build-envy && ctest --output-on-failure      (or ./envy_tests)
#
# When Wrath adds the root CMakeLists.txt, move the envy_engine sources into the
# team engine library and add envy_tests to the team test target.
cmake_minimum_required(VERSION 3.10)
project(tividy_envy CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TIVIDY_CODE ${CMAKE_CURRENT_SOURCE_DIR}/../../code)

add_library(envy_engine
    ${TIVIDY_CODE}/data/KeyValuePayload.cpp
    ${TIVIDY_CODE}/routing/SequentialRouting.cpp
    ${TIVIDY_CODE}/routing/RoutingCondition.cpp
    ${TIVIDY_CODE}/routing/DataBasedRouting.cpp
    ${TIVIDY_CODE}/progression/DependencyGraph.cpp
    ${TIVIDY_CODE}/progression/ProgressionTracker.cpp
)
target_include_directories(envy_engine PUBLIC
    ${TIVIDY_CODE}
    ${TIVIDY_CODE}/workflow
    ${TIVIDY_CODE}/data
    ${TIVIDY_CODE}/routing
    ${TIVIDY_CODE}/progression
)
if(MSVC)
    target_compile_options(envy_engine PRIVATE /W4)
else()
    target_compile_options(envy_engine PRIVATE -Wall -Wextra -pedantic)
endif()

enable_testing()
add_executable(envy_tests
    test_main.cpp
    test_routing.cpp
    test_dependencies.cpp
    test_progression.cpp
    test_integration.cpp
)
target_include_directories(envy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(envy_tests PRIVATE envy_engine)
add_test(NAME envy_tests COMMAND envy_tests)
