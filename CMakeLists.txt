cmake_minimum_required(VERSION 3.10)
project(RayWaves)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Always build raylib from source as a SHARED library so EXE and GameLogic share the same RLGL state
include(FetchContent)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP True
)
FetchContent_MakeAvailable(raylib)

# Collect source files
# Explicitly list Game sources for the editor to avoid pulling in game-only main
set(GAME_SRC_CPP
    Game/main.cpp
    Game/DllLoader.cpp
)
file(GLOB ENGINE_SRC_CPP  CONFIGURE_DEPENDS "Engine/*.cpp")
file(GLOB EDITOR_SRC_CPP  CONFIGURE_DEPENDS "Editor/*.cpp" "Editor/terminal/*.cpp")

# rlImGui and ImGui sources
set(RLIMGUI_SRC Editor/rlImGui/rlImGui.cpp)
set(IMGUI_SRC Editor/imgui/imgui.cpp Editor/imgui/imgui_draw.cpp Editor/imgui/imgui_tables.cpp Editor/imgui/imgui_widgets.cpp)
set(TINYFILEDIALOGS Editor/tinyfiledialogs/tinyfiledialogs.c)

# Copy assets to the build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/Assets")
    file(COPY ${CMAKE_SOURCE_DIR}/Assets DESTINATION ${CMAKE_BINARY_DIR})
endif()

# Build Engine as a static library so both main and GameLogic can link to it
add_library(Engine STATIC ${ENGINE_SRC_CPP})
# Expose Engine headers and link raylib publicly so consumers inherit include dirs/libs
target_include_directories(Engine PUBLIC 
    ${CMAKE_SOURCE_DIR}/Engine
)
target_link_libraries(Engine PUBLIC raylib)
target_link_libraries(Engine PRIVATE Dwmapi.lib)


# Create GameLogic shared library (DLL)
file(GLOB GAMELOGIC_SRC_CPP CONFIGURE_DEPENDS "GameLogic/*.cpp")
add_library(GameLogic SHARED ${GAMELOGIC_SRC_CPP})
# GameLogic needs Engine headers and linkage (for GameMap symbols)
target_include_directories(GameLogic PRIVATE 
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/GameLogic
)
target_link_libraries(GameLogic PRIVATE Engine)

# Create main executable (Game Editor) without Engine sources; link against Engine
add_executable(main WIN32
    ${GAME_SRC_CPP} 
    ${EDITOR_SRC_CPP} 
    ${RLIMGUI_SRC} 
    ${IMGUI_SRC} 
    ${TINYFILEDIALOGS}
    Editor/app.rc
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
if(MSVC)
    set_target_properties(main PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# Copy GameLogic DLL beside main executable after building 'main'.
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            $<TARGET_FILE_DIR:main>)

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
add_executable(game
    Game/game.cpp
    Game/DllLoader.cpp
    Game/app.rc
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

# Optional export packaging target
add_custom_target(export_package
    COMMAND ${CMAKE_COMMAND} -E echo "Running export script..."
    COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/Distribution/distribute.ps1" -BuildConfig Release -OutputDir dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging distribution via distribute.ps1"
)
