cmake_minimum_required(VERSION 3.25)

# Toolchain selection must happen before project() so CMake detects the Zig
# wrappers rather than the host compiler.
include(cmake/Toolchain.cmake)

project(RayWaves VERSION 0.1.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)

include(cmake/Platform.cmake)

# rcedit is only used to embed the Windows executable icon. Fetch it on demand
# so a fresh Windows checkout still produces branded binaries.
if(WIN32 AND NOT EXISTS "${CMAKE_SOURCE_DIR}/Tools/rcedit.exe")
    message(STATUS "rcedit not found. Auto-fetching...")
    execute_process(
        COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/Tools/setup_tools.ps1" -SkipZig -SkipNinja -SkipCMake
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE RCEDIT_FETCH_RESULT
    )
    if(NOT RCEDIT_FETCH_RESULT EQUAL 0)
        message(WARNING "Failed to download rcedit; icon embedding will be skipped.")
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
option(RAYWAVES_BUILD_GAMELOGIC "Build the bundled SlimeQuest GameLogic library" ON)

option(ENABLE_CLANG_TIDY "Run clang-tidy static analysis during build" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy
        HINTS "C:/Program Files/LLVM/bin" "/usr/bin" "/usr/local/bin"
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

include(cmake/Dependencies.cmake)
include(cmake/Engine.cmake)
include(cmake/GameLogic.cmake)
include(cmake/RuntimeSupport.cmake)
include(cmake/Editor.cmake)
include(cmake/Runtime.cmake)
include(cmake/Cli.cmake)
include(cmake/Packaging.cmake)

if(RAYWAVES_BUILD_TESTS)
    include(cmake/Tests.cmake)
endif()

if(RAYWAVES_BUILD_BENCHMARKS)
    include(cmake/Benchmarks.cmake)
endif()
