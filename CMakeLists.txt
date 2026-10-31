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

# ---- the engine: every .cpp under code/ except the interface's main() ----
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ${CODE}/*.cpp)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*/code/app/main\\.cpp$")
add_library(tividy_engine STATIC ${ENGINE_SOURCES})
tividy_configure(tividy_engine)

# ---- the interactive application (appears as soon as Lust adds code/app/main.cpp) ----
if(EXISTS ${CODE}/app/main.cpp)
  add_executable(tividy ${CODE}/app/main.cpp)
  target_link_libraries(tividy PRIVATE tividy_engine)
  tividy_configure(tividy)
endif()

# ---- tests (doctest; the team's doctest.h and the one main() live in tests/envy) ----
enable_testing()
set(DOCTEST_DIR ${CMAKE_SOURCE_DIR}/tests/envy)

file(GLOB_RECURSE ALL_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*/test_*.cpp)
add_executable(tividy_tests ${ALL_TESTS})
target_include_directories(tividy_tests PRIVATE ${DOCTEST_DIR})
target_link_libraries(tividy_tests PRIVATE tividy_engine)
tividy_configure(tividy_tests)
add_test(NAME tividy_tests COMMAND tividy_tests)

# Wrath's tests build from only the code they use, so they run before the whole engine compiles.
file(GLOB WRATH_SOURCES CONFIGURE_DEPENDS ${CODE}/approval/*.cpp ${CODE}/participants/*.cpp)
file(GLOB WRATH_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/wrath/test_*.cpp)
add_executable(wrath_tests ${WRATH_SOURCES} ${WRATH_TESTS} ${DOCTEST_DIR}/test_main.cpp)
target_include_directories(wrath_tests PRIVATE ${DOCTEST_DIR})
tividy_configure(wrath_tests)