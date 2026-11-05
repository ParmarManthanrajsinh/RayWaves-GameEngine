cmake_minimum_required(VERSION 3.10)

# Check and auto-fetch Zig if necessary before compiler detection runs
if(CMAKE_C_COMPILER MATCHES "zig-cc" OR CMAKE_CXX_COMPILER MATCHES "zig-cxx")
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/zig/zig.exe")
    message(STATUS "Zig toolchain not found. Auto-fetching...")
    execute_process(
            COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/Tools/setup_zig.ps1" -SkipRcEdit
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE ZIG_FETCH_RESULT
        )
    if(NOT ZIG_FETCH_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to download Zig.")
    endif()
  endif()
endif()

project(RayWaves VERSION 0.1.0 LANGUAGES C CXX)

# Auto-fetch rcedit if missing
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Tools/rcedit.exe")
  message(STATUS "rcedit not found. Auto-fetching...")
  execute_process(
        COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/Tools/setup_zig.ps1" -SkipZig
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE RCEDIT_FETCH_RESULT
    )
  if(NOT RCEDIT_FETCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to download rcedit.")
  endif()
endif()

# Option: strip Profiler + SCOPED_TIMER from release builds
# Enable with: cmake -B build/zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
option(RAYWAVES_DISTRIBUTION_BUILD "Strip profiler and debug tooling from release build" OFF)
if(RAYWAVES_DISTRIBUTION_BUILD)
    message(STATUS "Profiler disabled for distribution build")
endif()

option(RAYWAVES_BUILD_TESTS "Build unit, integration and smoke test suites" ON)
option(RAYWAVES_BUILD_BENCHMARKS "Build the standalone benchmark suite" ON)
option(RAYWAVES_BUILD_GAMELOGIC "Build the bundled SlimeQuest GameLogic DLL" ON)

option(ENABLE_CLANG_TIDY "Run clang-tidy static analysis during build" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy
        HINTS "C:/Program Files/LLVM/bin"
        NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
    )
    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY=ON but clang-tidy not found in PATH")
    endif()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Zig/Clang flags only. MSVC does not understand these; the presets use Zig.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-msse4.2)
    add_compile_options(-fno-sanitize=all)
    add_compile_options(-Qunused-arguments)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-msse4.2)
endif()

add_compile_definitions(IMGUI_DISABLE_SSE)

include(cmake/Dependencies.cmake)
include(cmake/Engine.cmake)
include(cmake/GameLogic.cmake)
include(cmake/Editor.cmake)
include(cmake/Runtime.cmake)
include(cmake/Packaging.cmake)

if(RAYWAVES_BUILD_TESTS)
    include(cmake/Tests.cmake)
endif()

if(RAYWAVES_BUILD_BENCHMARKS)
    include(cmake/Benchmarks.cmake)
endif()
