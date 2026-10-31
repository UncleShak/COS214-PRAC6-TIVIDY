# Lust's interface tests and the development-only program.
#
#   cmake -S tests/lust -B build-lust
#   cmake --build build-lust
#   ctest --test-dir build-lust --output-on-failure     (or ./build-lust/lust_tests)
#   ./build-lust/tividy_dev                              (UI on the FAKE engine)

cmake_minimum_required(VERSION 3.10)
project(lust_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory(${REPO_ROOT}/code/interface ${CMAKE_CURRENT_BINARY_DIR}/interface)

# Fake engine: development and tests only, never part of the shipped program.
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
)
target_include_directories(lust_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lust_tests lust_fake tividy_ui_core)

enable_testing()
add_test(NAME lust_tests COMMAND lust_tests)

add_executable(tividy_dev ${TIVIDY_MAIN_SOURCE} fake/FakeEngineFactory.cpp)
target_link_libraries(tividy_dev lust_fake ${TIVIDY_FRONT_ENDS})
