cmake_minimum_required(VERSION 3.10)

project(RayWaves)

# Option: strip Profiler + SCOPED_TIMER from release builds
# Enable with: cmake --preset linux-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
option(RAYWAVES_DISTRIBUTION_BUILD "Strip profiler and debug tooling from release build" OFF)
if(RAYWAVES_DISTRIBUTION_BUILD)
    message(STATUS "Profiler disabled for distribution build")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SSE only exists on x86; aarch64 rejects the flag outright.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
    add_compile_options(-msse4.2)
endif()
option(RAYWAVES_SANITIZERS "Enable ASan/UBSan instrumentation" OFF)
if(RAYWAVES_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
else()
    add_compile_options(-fno-sanitize=all)
endif()
# -Qunused-arguments is Clang-only; GCC errors on it.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Qunused-arguments)
endif()


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

# Stage raylib headers + static lib alongside exe so generated
# per-project CMakeLists.txt can find them with exe-relative path.
set(RAYLIB_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/raylib-build/raylib")
set(RAYLIB_STAGEDIR "${CMAKE_BINARY_DIR}/raylib")
file(MAKE_DIRECTORY "${RAYLIB_STAGEDIR}/include" "${RAYLIB_STAGEDIR}/lib")
foreach(HDR raylib.h raymath.h rcamera.h rlgl.h)
    if(EXISTS "${RAYLIB_BUILD_DIR}/include/${HDR}")
        file(COPY "${RAYLIB_BUILD_DIR}/include/${HDR}" DESTINATION "${RAYLIB_STAGEDIR}/include/")
    endif()
endforeach()
# The shared library itself is staged at build time by the stage_raylib
# target below: a configure-time file(COPY) runs before raylib exists on a
# fresh tree, and copying only one of the chain names silently links test
# plugins against a system-installed libraylib instead of the bundled one.

# Stage the full libraylib chain (real file + soname link + unversioned
# alias) into the build dir and the raylib stage dirs. Only soname alias is
# what NEEDED entries resolve at runtime; only libraylib.so is what -L/-l
# find at link time. All three names or linking/loading picks up whatever
# happens to be installed system-wide.
add_custom_target(stage_raylib ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}" "${RAYLIB_STAGEDIR}/bin" "${RAYLIB_STAGEDIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:raylib>"
            "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:raylib>"
            "${CMAKE_BINARY_DIR}/libraylib.so"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${CMAKE_BINARY_DIR}/$<TARGET_SONAME_FILE_NAME:raylib>"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${CMAKE_BINARY_DIR}/libraylib.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:raylib>"
            "${RAYLIB_STAGEDIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "${RAYLIB_STAGEDIR}/bin/$<TARGET_SONAME_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/bin/libraylib.so"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/bin/$<TARGET_SONAME_FILE_NAME:raylib>"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/bin/libraylib.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:raylib>"
            "${RAYLIB_STAGEDIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "${RAYLIB_STAGEDIR}/lib/$<TARGET_SONAME_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/lib/libraylib.so"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/lib/$<TARGET_SONAME_FILE_NAME:raylib>"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:raylib>"
            "${RAYLIB_STAGEDIR}/lib/libraylib.so"
    DEPENDS raylib
    COMMENT "Staging libraylib chain (real + soname + alias)"
)

# Collect source files
# Explicitly list Game sources for the editor to avoid pulling in game-only main
set(GAME_SRC_CPP
    Game/main.cpp
    Game/DllLoader.cpp
)
set(ENGINE_SRC_CPP
    Engine/AssetResolver.cpp
    Engine/GameConfig.cpp
    Engine/GameEngine.cpp
    Engine/GameMap.cpp
    Engine/MapManager.cpp
    Engine/Profiler.cpp
    Engine/Project.cpp
    Engine/ProjectManager.cpp
    Engine/raygui_impl.cpp
    Engine/WindowUtils.cpp
)
set(EDITOR_SRC_CPP
    Editor/EditorPreferences.cpp
    Editor/EditorUtils.cpp
    Editor/FileAssociation.cpp
    Editor/GameEditor.cpp
    Editor/GameLogicLoader.cpp
    Editor/ThemeService.cpp
    Editor/ExportService.cpp
    Editor/ProcessRunner.cpp
    Editor/terminal/terminal.cpp
    Editor/Panels/EditorPreferencesPanel.cpp
    Editor/Panels/ExportPanel.cpp
    Editor/Panels/MainMenuBar.cpp
    Editor/Panels/MapSelectionPanel.cpp
    Editor/Panels/MessageLogPanel.cpp
    Editor/Panels/PerformanceOverlay.cpp
    Editor/Panels/SceneSettingsPanel.cpp
    Editor/Panels/SceneWindow.cpp
)

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

# Copy demo assets from the SlimeQuest template to the build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/Distribution/Templates/SlimeQuest/Assets")
  file(COPY ${CMAKE_SOURCE_DIR}/Distribution/Templates/SlimeQuest/Assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Build Engine as a static library so both main and GameLogic can link to it.
# GameLogic.so relinks Engine objects, so they must be PIC.
add_library(Engine STATIC ${ENGINE_SRC_CPP})
set_target_properties(Engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Expose Engine headers and link raylib publicly so consumers inherit include dirs/libs
target_include_directories(Engine PUBLIC
    ${CMAKE_SOURCE_DIR}/Engine
)
target_link_libraries(Engine PUBLIC raylib)


# Create GameLogic shared library (DLL)
set(GAMELOGIC_SRC_CPP
    Distribution/Templates/SlimeQuest/GameLogic/DemoLevel.cpp
    Distribution/Templates/SlimeQuest/GameLogic/DemoMainMenu.cpp
    Distribution/Templates/SlimeQuest/GameLogic/GameCamera.cpp
    Distribution/Templates/SlimeQuest/GameLogic/Player.cpp
    Distribution/Templates/SlimeQuest/GameLogic/RootManager.cpp
    Distribution/Templates/SlimeQuest/GameLogic/Slime.cpp
)
add_library(GameLogic SHARED ${GAMELOGIC_SRC_CPP})
set_target_properties(GameLogic PROPERTIES PREFIX "")
# Linux: dlopen("GameLogic.so") must resolve libraylib.so without
# LD_LIBRARY_PATH. $ORIGIN keeps it working wherever the plugin sits next to
# the staged chain (build dir, dist root, exported game folder).
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set_target_properties(GameLogic PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)
# GameLogic needs Engine headers and linkage (for GameMap symbols)
target_include_directories(GameLogic PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Distribution/Templates/SlimeQuest/GameLogic
)
target_link_libraries(GameLogic PRIVATE Engine)

add_executable(main
    ${GAME_SRC_CPP}
    ${EDITOR_SRC_CPP}
    ${RLIMGUI_SRC}
    ${IMGUI_SRC}
    ${TINYFILEDIALOGS}
)
set_target_properties(main PROPERTIES OUTPUT_NAME "RayWaves")
# Binaries locate the staged libraylib chain beside themselves, so the dist
# copy works without LD_LIBRARY_PATH or a build-tree path baked in.
set_target_properties(main PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(main PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()
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

# libraylib staging handled by the stage_raylib target (chain of three names).
add_dependencies(main stage_raylib)

# When building GameLogic alone, also copy it next to the main executable
add_custom_command(TARGET GameLogic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            ${CMAKE_BINARY_DIR})

# ------------------------------
# Game-only runtime executable
# ------------------------------
add_executable(game
    Game/game.cpp
    Game/DllLoader.cpp
)
set_target_properties(game PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(game PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()
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

# libraylib staging handled by stage_raylib.
add_dependencies(game stage_raylib)

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

# Copy GameLogic beside smoketest
add_custom_command(TARGET smoketest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GameLogic> $<TARGET_FILE_DIR:smoketest>
)
add_dependencies(smoketest stage_raylib)

# Unit test target
add_executable(tests
    Tests/main.cpp
    Tests/DllLoader_t.cpp
    Tests/GameConfig_t.cpp
    Tests/Project_t.cpp
    Tests/AssetResolver_t.cpp
    Tests/StateBag_t.cpp
    Tests/ReloadRoundtrip_t.cpp
    Tests/ProjectManager_t.cpp
    Tests/Profiler_t.cpp
    Tests/GameMap_t.cpp
    Tests/MapManager_t.cpp
    Tests/GameEditor_t.cpp
    Tests/PanelRegistry_t.cpp
    Tests/ExportService_t.cpp
    Tests/PerfBenchmark_t.cpp
    Game/DllLoader.cpp
    Editor/ExportService.cpp
    Editor/EditorUtils.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Game
    ${CMAKE_SOURCE_DIR}/Editor
)

target_link_libraries(tests PRIVATE Engine)

add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GameLogic> $<TARGET_FILE_DIR:tests>
)
# tests links -lraylib against the staged dir; order after stage_raylib.
add_dependencies(tests stage_raylib)

enable_testing()
add_test(NAME unit COMMAND tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_test(NAME smoke COMMAND smoketest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Optional export packaging target
add_custom_target(export_package
    COMMAND ${CMAKE_COMMAND} -E echo "Running export script..."
    COMMAND "${CMAKE_SOURCE_DIR}/Distribution/distribute.sh" -BuildConfig Release -OutputDir dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging distribution via distribute.sh"
)
