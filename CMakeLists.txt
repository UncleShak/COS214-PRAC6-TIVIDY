# TIVIDY root build.
#
#   cmake -S . -B build
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# Nobody edits this file to add a source file: every .cpp under src/ and every
# tests/{unit,integration}/test_*.cpp is picked up.
cmake_minimum_required(VERSION 3.12)
project(TIVIDY LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(TIVIDY_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

set(CODE ${CMAKE_SOURCE_DIR}/src)

# Headers are included by file name (#include "WorkItem.h"), so every engine folder under src/
# is on the include path. The interface (src/ui) sets up its own.
set(CODE_DIRS ${CODE})
file(GLOB_RECURSE CODE_CHILDREN LIST_DIRECTORIES true ${CODE}/*)
foreach(child ${CODE_CHILDREN})
  if(IS_DIRECTORY ${child} AND NOT child MATCHES "/src/ui(/|$)")
    list(APPEND CODE_DIRS ${child})
  endif()
endforeach()

function(tividy_configure target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    if(TIVIDY_SANITIZE)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_libraries(${target} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
  target_include_directories(${target} PRIVATE ${CODE_DIRS})
endfunction()

# ---- the engine: every .cpp under src/ except the interface and main.cpp ----
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ${CODE}/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/src/ui/.*")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
add_library(tividy_engine STATIC ${ENGINE_SOURCES})
tividy_configure(tividy_engine)

# ---- the interactive application and its UI test harness ----
# tests/ui pulls in src/ui, which builds the shipped program, tividy, against the real engine.
# ui_tests is a UI-only harness on a FAKE engine (it never touches the engine).
add_subdirectory(tests/ui)

# ---- tests (doctest; doctest.h and the shared main() live in tests/support) ----
enable_testing()
set(TEST_SUPPORT_DIR ${CMAKE_SOURCE_DIR}/tests/support)

# Unit and integration tests against the real engine.
file(GLOB ENGINE_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/unit/test_*.cpp
                                         ${CMAKE_SOURCE_DIR}/tests/integration/test_*.cpp)
add_executable(tividy_tests ${ENGINE_TESTS} ${TEST_SUPPORT_DIR}/test_main.cpp)
target_include_directories(tividy_tests PRIVATE ${TEST_SUPPORT_DIR})
target_link_libraries(tividy_tests PRIVATE tividy_engine)
tividy_configure(tividy_tests)
add_test(NAME tividy_tests COMMAND tividy_tests)

# The real gateway the interface uses (src/ui/real), tested against the real engine.
file(GLOB GATEWAY_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/gateway/test_*.cpp)
add_executable(gateway_tests ${GATEWAY_TESTS} ${TEST_SUPPORT_DIR}/test_main.cpp)
target_include_directories(gateway_tests PRIVATE ${TEST_SUPPORT_DIR})
target_link_libraries(gateway_tests PRIVATE tividy_real_engine)
tividy_configure(gateway_tests)
add_test(NAME gateway_tests COMMAND gateway_tests)

# Stress / robustness tests against the real engine. Not in CTest: some failures document
# known engine limitations on purpose.
file(GLOB STRESS_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/stress/test_*.cpp)
add_executable(stress_tests ${STRESS_TESTS} ${TEST_SUPPORT_DIR}/test_main.cpp)
target_include_directories(stress_tests PRIVATE ${TEST_SUPPORT_DIR} ${CODE}/ui)
target_link_libraries(stress_tests PRIVATE tividy_engine tividy_real_engine)
tividy_configure(stress_tests)

# The demo scripts run through the shipped program in --plain mode and must finish cleanly.
file(GLOB DEMO_SCRIPTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/e2e/*.txt)
foreach(script ${DEMO_SCRIPTS})
  get_filename_component(name ${script} NAME_WE)
  add_test(NAME demo_${name}
           COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:tividy> -DSCRIPT=${script}
                   -P ${CMAKE_SOURCE_DIR}/tests/e2e/run_script.cmake)
endforeach()
