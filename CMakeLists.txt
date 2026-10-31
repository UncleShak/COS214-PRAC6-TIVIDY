# TIVIDY root build.
#
#   cmake -S . -B build
#   cmake --build build                       (everything, once the whole engine compiles)
#   cmake --build build --target wrath_tests  (Wrath's tests alone: they compile before the engine is complete)
#   cd build && ctest --output-on-failure     (or ./tividy_tests)
#
# Nobody edits this file to add a source file: every .cpp under code/ and tests/ is picked up.
cmake_minimum_required(VERSION 3.12)
project(TIVIDY LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(TIVIDY_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

set(CODE ${CMAKE_SOURCE_DIR}/code)

# Headers are included by file name (#include "WorkItem.h"), so code/ and each folder in it is on the include path.
set(CODE_DIRS ${CODE})
file(GLOB CODE_CHILDREN RELATIVE ${CODE} ${CODE}/*)
foreach(child ${CODE_CHILDREN})
  if(IS_DIRECTORY ${CODE}/${child})
    list(APPEND CODE_DIRS ${CODE}/${child})
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

# ---- the engine: every .cpp under code/ except the interface (Lust builds that, below) ----
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ${CODE}/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/code/app/.*")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/code/interface/.*")
add_library(tividy_engine STATIC ${ENGINE_SOURCES})
tividy_configure(tividy_engine)

# ---- the interactive application and its UI-only test harness (Lust) ----
# tests/lust pulls in code/interface, which builds the shipped program, tividy, against the real
# engine. lust_tests is a UI-only harness on a FAKE engine (flagged: it never touches the engine).
add_subdirectory(tests/lust)

# ---- tests (doctest; the team's doctest.h and the one main() live in tests/envy) ----
enable_testing()
set(DOCTEST_DIR ${CMAKE_SOURCE_DIR}/tests/envy)

file(GLOB_RECURSE ALL_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*/test_*.cpp)
list(FILTER ALL_TESTS EXCLUDE REGEX ".*/tests/lust/.*")      # own executable and main(), see above
list(FILTER ALL_TESTS EXCLUDE REGEX ".*/tests/gateway/.*")   # own executable, see below
add_executable(tividy_tests ${ALL_TESTS})
target_include_directories(tividy_tests PRIVATE ${DOCTEST_DIR})
target_link_libraries(tividy_tests PRIVATE tividy_engine)
tividy_configure(tividy_tests)
add_test(NAME tividy_tests COMMAND tividy_tests)

# Wrath's tests build from only the code they use, so they run before the whole engine compiles.
file(GLOB WRATH_SOURCES CONFIGURE_DEPENDS ${CODE}/approval/*.cpp ${CODE}/participants/*.cpp)
file(GLOB WRATH_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/wrath/test_*.cpp)
list(FILTER WRATH_TESTS EXCLUDE REGEX ".*/test_invalid_operations\\.cpp$")   # needs WorkItem: runs in tividy_tests
add_executable(wrath_tests ${WRATH_SOURCES} ${WRATH_TESTS} ${DOCTEST_DIR}/test_main.cpp)
target_include_directories(wrath_tests PRIVATE ${DOCTEST_DIR})
tividy_configure(wrath_tests)

# The real gateway the interface uses (code/interface/real), tested against the real engine.
file(GLOB GATEWAY_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/gateway/test_*.cpp)
add_executable(gateway_tests ${GATEWAY_TESTS} ${DOCTEST_DIR}/test_main.cpp)
target_include_directories(gateway_tests PRIVATE ${DOCTEST_DIR})
target_link_libraries(gateway_tests PRIVATE tividy_real_engine)
tividy_configure(gateway_tests)
add_test(NAME gateway_tests COMMAND gateway_tests)

# The demo scripts run through the shipped program in --plain mode and must finish cleanly.
file(GLOB DEMO_SCRIPTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/scripts/*.txt)
foreach(script ${DEMO_SCRIPTS})
  get_filename_component(name ${script} NAME_WE)
  add_test(NAME demo_${name}
           COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:tividy> -DSCRIPT=${script}
                   -P ${CMAKE_SOURCE_DIR}/tests/scripts/run_script.cmake)
endforeach()
