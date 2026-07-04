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

project(RayWaves)

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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add_compile_options(/fsanitize=address)
# add_link_options(/fsanitize=address)

add_compile_options(-msse4.2)
add_compile_options(-fno-sanitize=all)
add_compile_options(-Qunused-arguments)
add_compile_definitions(IMGUI_DISABLE_SSE)

# Always build raylib from source as a SHARED library so EXE and GameLogic share the same RLGL state
include(FetchContent)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Raylib examples to speed up build" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable Raylib tests" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP True
)
set(CMAKE_C_FLAGS_BACKUP "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -Wno-unused-command-line-argument")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-unused-command-line-argument")
FetchContent_MakeAvailable(raylib)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BACKUP}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")
unset(CMAKE_C_FLAGS_BACKUP)
unset(CMAKE_CXX_FLAGS_BACKUP)

# Collect source files
# Explicitly list Game sources for the editor to avoid pulling in game-only main
set(GAME_SRC_CPP
    Game/main.cpp
    Game/DllLoader.cpp
)
file(GLOB ENGINE_SRC_CPP  CONFIGURE_DEPENDS "Engine/*.cpp")
file(GLOB EDITOR_SRC_CPP  CONFIGURE_DEPENDS "Editor/*.cpp" "Editor/terminal/*.cpp" "Editor/Panels/*.cpp" "Editor/EditorUtils.cpp")

# rlImGui and ImGui sources
set(RLIMGUI_SRC Editor/rlImGui/rlImGui.cpp)
set(
    IMGUI_SRC Editor/imgui/imgui.cpp
    Editor/imgui/imgui_draw.cpp
    Editor/imgui/imgui_tables.cpp
    Editor/imgui/imgui_widgets.cpp
    Editor/imgui/imgui_stdlib.cpp
)
set(TINYFILEDIALOGS Editor/tinyfiledialogs/tinyfiledialogs.c)

# Copy engine content to the build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/EngineContent")
  file(COPY ${CMAKE_SOURCE_DIR}/EngineContent DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/EngineContent/config.ini")
  file(COPY ${CMAKE_SOURCE_DIR}/EngineContent/config.ini DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Copy demo assets from the DemoGame template to the build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/Distribution/Templates/DemoGame/Assets")
  file(COPY ${CMAKE_SOURCE_DIR}/Distribution/Templates/DemoGame/Assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Build Engine as a static library so both main and GameLogic can link to it
add_library(Engine STATIC ${ENGINE_SRC_CPP})
# Expose Engine headers and link raylib publicly so consumers inherit include dirs/libs
target_include_directories(Engine PUBLIC
    ${CMAKE_SOURCE_DIR}/Engine
)
target_link_libraries(Engine PUBLIC raylib)
target_link_libraries(Engine PRIVATE dwmapi)


# Create GameLogic shared library (DLL)
file(GLOB GAMELOGIC_SRC_CPP CONFIGURE_DEPENDS "Distribution/Templates/DemoGame/GameLogic/*.cpp")
add_library(GameLogic SHARED ${GAMELOGIC_SRC_CPP})
set_target_properties(GameLogic PROPERTIES PREFIX "")
# GameLogic needs Engine headers and linkage (for GameMap symbols)
target_include_directories(GameLogic PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Distribution/Templates/DemoGame/GameLogic
)
target_link_libraries(GameLogic PRIVATE Engine)

add_executable(main WIN32
    ${GAME_SRC_CPP}
    ${EDITOR_SRC_CPP}
    ${RLIMGUI_SRC}
    ${IMGUI_SRC}
    ${TINYFILEDIALOGS}
)
set_target_properties(main PROPERTIES OUTPUT_NAME "RayWaves")
target_link_options(main PRIVATE -Wl,--subsystem,windows)
if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(main PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:main> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
)
# Add include directories for the main executable
target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/imgui
    ${CMAKE_SOURCE_DIR}/Editor/rlImGui
    ${CMAKE_SOURCE_DIR}/Editor/tinyfiledialogs
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Editor
    ${CMAKE_SOURCE_DIR}/Game
)

# Link libraries
# Link Engine (which brings raylib transitively)
target_link_libraries(main PRIVATE Engine)

# Copy GameLogic DLL beside main executable after building 'main'.
# add_custom_command(TARGET main POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#             $<TARGET_FILE:GameLogic>
#             $<TARGET_FILE_DIR:main>)

# Also copy raylib DLL so both EXE and GameLogic load the same shared raylib
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:main>)

# When building GameLogic alone, also copy it next to the main executable
add_custom_command(TARGET GameLogic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            ${CMAKE_BINARY_DIR})

# ------------------------------
# Game-only runtime executable
# ------------------------------
add_executable(game WIN32
    Game/game.cpp
    Game/DllLoader.cpp
)
target_link_options(game PRIVATE -Wl,--subsystem,windows)
if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(game PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:game> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
)
# Includes for game runtime
target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Game
)

# Link Engine (transitively links raylib)
target_link_libraries(game PRIVATE Engine)

# Copy GameLogic DLL beside game executable after building 'game'.
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            $<TARGET_FILE_DIR:game>)

# Also copy raylib DLL so both EXE and GameLogic load the same shared raylib
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:game>)

# ------------------------------
# Smoke Test executable
# ------------------------------
add_executable(smoketest
    Tests/SmokeTest.cpp
    Game/DllLoader.cpp
)

target_include_directories(smoketest PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Game
)

target_link_libraries(smoketest PRIVATE Engine)
if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(smoketest PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

# Copy GameLogic and raylib DLLs beside smoketest
add_custom_command(TARGET smoketest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GameLogic> $<TARGET_FILE_DIR:smoketest>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:raylib> $<TARGET_FILE_DIR:smoketest>
)

# Unit test target
add_executable(tests
    Tests/main.cpp
    Tests/GameConfig_t.cpp
    Tests/Project_t.cpp
    Tests/AssetResolver_t.cpp
    Tests/StateBag_t.cpp
    Tests/ProjectManager_t.cpp
    Tests/Profiler_t.cpp
    Tests/GameMap_t.cpp
    Tests/MapManager_t.cpp
    Tests/PerfBenchmark_t.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(tests PRIVATE Engine)

add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:raylib> $<TARGET_FILE_DIR:tests>
)

enable_testing()
add_test(NAME unit COMMAND tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME smoke COMMAND smoketest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Optional export packaging target
add_custom_target(export_package
    COMMAND ${CMAKE_COMMAND} -E echo "Running export script..."
    COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/Distribution/distribute.ps1" -BuildConfig Release -OutputDir dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging distribution via distribute.ps1"
)
