# TIVIDY interactive interface (Lust).
#
# Targets:
#   tividy_ui_core  controller, validation, log (no ncurses; unit-tested in tests/lust)
#   tividy_tui      ncurses front end (only with TIVIDY_WITH_CURSES)
#   tividy          the shipped program, built once RealEngineFactory.cpp exists
#
# The development program (tividy_dev, linked to the fake engine) is defined in
# tests/lust/CMakeLists.txt so the fake can never end up in the shipped program.

cmake_minimum_required(VERSION 3.10)
project(tividy_interface CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TIVIDY_WITH_CURSES "Build the ncurses front end (off: --plain only)" ON)

get_filename_component(TIVIDY_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

add_library(tividy_ui_core STATIC
    InterfaceController.cpp
    InputValidation.cpp
    LineInterpreter.cpp
    Log.cpp
    plain/PlainFrontEnd.cpp
    ${TIVIDY_CODE_DIR}/iterator/HistoryIterator.cpp   # Envy's Iterator, used for history
)
target_include_directories(tividy_ui_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/plain
    ${TIVIDY_CODE_DIR}/iterator
)
target_compile_options(tividy_ui_core PRIVATE -Wall -Wextra)

if(TIVIDY_WITH_CURSES)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses REQUIRED)

    add_library(tividy_tui STATIC
        tui/CursesSession.cpp
        tui/Layout.cpp
        tui/Theme.cpp
        tui/TuiFrontEnd.cpp
    )
    target_include_directories(tividy_tui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tui ${CURSES_INCLUDE_DIRS})
    target_compile_definitions(tividy_tui PUBLIC TIVIDY_WITH_CURSES NCURSES_WIDECHAR=1)
    target_link_libraries(tividy_tui PUBLIC tividy_ui_core ${CURSES_LIBRARIES})
    target_compile_options(tividy_tui PRIVATE -Wall -Wextra)
    set(TIVIDY_FRONT_ENDS tividy_tui)
else()
    set(TIVIDY_FRONT_ENDS tividy_ui_core)
endif()

set(TIVIDY_MAIN_SOURCE ${TIVIDY_CODE_DIR}/app/main.cpp)   # entry point agreed by the team
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(TIVIDY_MAIN_SOURCE ${TIVIDY_MAIN_SOURCE} PARENT_SCOPE)
    set(TIVIDY_FRONT_ENDS ${TIVIDY_FRONT_ENDS} PARENT_SCOPE)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/RealEngineFactory.cpp)
    add_executable(tividy ${TIVIDY_MAIN_SOURCE} RealEngineFactory.cpp)
    target_link_libraries(tividy ${TIVIDY_FRONT_ENDS})
else()
    message(STATUS "tividy: not built yet, RealEngineFactory.cpp (real engine) does not exist")
endif()
