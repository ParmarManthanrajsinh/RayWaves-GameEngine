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

set(RLIMGUI_SRC Editor/rlImGui/rlImGui.cpp)
set(IMGUI_SRC
    Editor/imgui/imgui.cpp
    Editor/imgui/imgui_draw.cpp
    Editor/imgui/imgui_tables.cpp
    Editor/imgui/imgui_widgets.cpp
    Editor/imgui/imgui_stdlib.cpp
)
set(TINYFILEDIALOGS Editor/tinyfiledialogs/tinyfiledialogs.c)

add_executable(main WIN32
    Runtime/main.cpp
    Runtime/DllLoader.cpp
    Runtime/PeCrtCheck.cpp
    ${EDITOR_SRC_CPP}
    ${RLIMGUI_SRC}
    ${IMGUI_SRC}
    ${TINYFILEDIALOGS}
    EngineContent/app.rc
)
add_executable(raywaves::editor ALIAS main)
set_target_properties(main PROPERTIES OUTPUT_NAME "RayWaves")
target_link_options(main PRIVATE -Wl,--subsystem,windows)

if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(main PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

target_include_directories(main PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/imgui
    ${CMAKE_SOURCE_DIR}/Editor/rlImGui
    ${CMAKE_SOURCE_DIR}/Editor/tinyfiledialogs
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Editor
    ${CMAKE_SOURCE_DIR}/Runtime
)

target_link_libraries(main PRIVATE Engine)
add_dependencies(main stage_raylib)

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:main> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:main>
)
