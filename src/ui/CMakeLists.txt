# TIVIDY interactive interface.
#
# Targets:
#   tividy_ui_core  controller, validation, log (no ncurses; unit-tested in tests/ui)
#   tividy_tui      ncurses front end (only with TIVIDY_WITH_CURSES)
#   tividy          the shipped program, built once RealEngineFactory.cpp exists
#
# The fake engine used by the UI tests lives in tests/ui, so it can never end up in the
# shipped program.

cmake_minimum_required(VERSION 3.10)
project(tividy_interface CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TIVIDY_WITH_CURSES "Build the ncurses front end (off: --plain only)" ON)

get_filename_component(TIVIDY_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(TIVIDY_UI_CORE_SOURCES
    InterfaceController.cpp
    InputValidation.cpp
    LineInterpreter.cpp
    Log.cpp
    plain/PlainFrontEnd.cpp
    tui/RouteModel.cpp
)
# The history Iterator comes from the engine library in the full build; built alone (tests/ui),
# the interface compiles that one file itself.
if(NOT TARGET tividy_engine)
    list(APPEND TIVIDY_UI_CORE_SOURCES ${TIVIDY_CODE_DIR}/execution/traversal/HistoryIterator.cpp)
endif()
add_library(tividy_ui_core STATIC ${TIVIDY_UI_CORE_SOURCES})
target_include_directories(tividy_ui_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/plain
    ${TIVIDY_CODE_DIR}/execution/traversal
)
target_compile_options(tividy_ui_core PRIVATE -Wall -Wextra)
if(TARGET tividy_engine)
    target_link_libraries(tividy_ui_core PUBLIC tividy_engine)
endif()

if(TIVIDY_WITH_CURSES)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses REQUIRED)

    add_library(tividy_tui STATIC
        tui/CursesSession.cpp
        tui/Layout.cpp
        tui/RouteLine.cpp
        tui/Theme.cpp
        tui/TuiFrontEnd.cpp
        tui/Widgets.cpp
    )
    target_include_directories(tividy_tui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tui ${CURSES_INCLUDE_DIRS})
    target_compile_definitions(tividy_tui PUBLIC TIVIDY_WITH_CURSES NCURSES_WIDECHAR=1)
    target_link_libraries(tividy_tui PUBLIC tividy_ui_core ${CURSES_LIBRARIES})
    target_compile_options(tividy_tui PRIVATE -Wall -Wextra)
    set(TIVIDY_FRONT_ENDS tividy_tui)
else()
    set(TIVIDY_FRONT_ENDS tividy_ui_core)
endif()

set(TIVIDY_MAIN_SOURCE ${TIVIDY_CODE_DIR}/main.cpp)
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(TIVIDY_MAIN_SOURCE ${TIVIDY_MAIN_SOURCE} PARENT_SCOPE)
    set(TIVIDY_FRONT_ENDS ${TIVIDY_FRONT_ENDS} PARENT_SCOPE)
endif()

# The shipped program: the real engine gateway, only inside the full build (root CMakeLists.txt).
if(TARGET tividy_engine)
    add_library(tividy_real_engine STATIC real/RealEngine.cpp)
    target_include_directories(tividy_real_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/real)
    target_link_libraries(tividy_real_engine PUBLIC tividy_engine)
    tividy_configure(tividy_real_engine)

    add_executable(tividy ${TIVIDY_MAIN_SOURCE} RealEngineFactory.cpp)
    target_link_libraries(tividy PRIVATE tividy_real_engine ${TIVIDY_FRONT_ENDS})
    tividy_configure(tividy)
    set_target_properties(tividy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
else()
    message(STATUS "tividy: built only from the root CMakeLists.txt (it needs the engine)")
endif()
