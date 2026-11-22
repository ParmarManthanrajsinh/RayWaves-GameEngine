set(EDITOR_SRC_CPP
    Editor/Autosave.cpp
    Editor/CommandStack.cpp
    Editor/SceneCommands.cpp
    Editor/EditorAssets.cpp
    Editor/EditorPreferences.cpp
    Editor/GameEditor.cpp
    Editor/GameLogicLoader.cpp
    Editor/ThemeService.cpp
    Editor/ExportService.cpp
    Editor/terminal/terminal.cpp
    Editor/terminal/terminal_view.cpp
    Editor/Panels/EditorPreferencesPanel.cpp
    Editor/Panels/ExportPanel.cpp
    Editor/Panels/FileSystemPanel.cpp
    Editor/Panels/InspectorPanel.cpp
    Editor/Panels/MainMenuBar.cpp
    Editor/Panels/MessageLogPanel.cpp
    Editor/Panels/PerformanceOverlay.cpp
    Editor/Panels/SceneHierarchyPanel.cpp
    Editor/Panels/SceneSettingsPanel.cpp
    Editor/Panels/SceneWindow.cpp
    Editor/Panels/TerminalPanel.cpp
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

set(LIBVTERM_SRC
    third_party/libvterm/src/encoding.c
    third_party/libvterm/src/keyboard.c
    third_party/libvterm/src/mouse.c
    third_party/libvterm/src/parser.c
    third_party/libvterm/src/pen.c
    third_party/libvterm/src/screen.c
    third_party/libvterm/src/state.c
    third_party/libvterm/src/unicode.c
    third_party/libvterm/src/vterm.c
)

# Terminal core: no ImGui/editor dependencies so unit and integration tests
# can link it directly.
add_library(raywaves_terminal_core STATIC
    ${RAYWAVES_EDITOR_PLATFORM_DIR}/pty_session.cpp
    Editor/terminal/shell_detect.cpp
    Editor/terminal/terminal_session.cpp
    Editor/terminal/vterm_screen.cpp
    ${LIBVTERM_SRC}
)
target_include_directories(raywaves_terminal_core PUBLIC
    ${CMAKE_SOURCE_DIR}/Editor/terminal
    ${CMAKE_SOURCE_DIR}/third_party/libvterm/include
)
target_include_directories(raywaves_terminal_core PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/libvterm/src
)
target_link_libraries(raywaves_terminal_core PUBLIC Threads::Threads)

set(RAYWAVES_EDITOR_PLATFORM_SRC
    ${RAYWAVES_EDITOR_PLATFORM_DIR}/EditorUtils.cpp
    ${RAYWAVES_EDITOR_PLATFORM_DIR}/FileAssociation.cpp
)

set(RAYWAVES_EDITOR_RC "")
if(WIN32)
    set(RAYWAVES_EDITOR_RC EngineContent/app.rc)
endif()

add_executable(main WIN32
    Runtime/main.cpp
    ${EDITOR_SRC_CPP}
    ${RAYWAVES_EDITOR_PLATFORM_SRC}
    ${RLIMGUI_SRC}
    ${IMGUI_SRC}
    ${TINYFILEDIALOGS}
    ${RAYWAVES_EDITOR_RC}
)
add_executable(raywaves::editor ALIAS main)
set_target_properties(main PROPERTIES OUTPUT_NAME "RayWaves")
raywaves_set_origin_rpath(main)

if(WIN32)
    target_link_options(main PRIVATE -Wl,--subsystem,windows)
endif()

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
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(main PRIVATE Engine RuntimeSupport raywaves_terminal_core Threads::Threads)
add_dependencies(main stage_raylib)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Tools/rcedit.exe")
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:main> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
    )
endif()
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:main>
)
if(UNIX)
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_SONAME_FILE:raylib>" "$<TARGET_FILE_DIR:main>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_LINKER_FILE:raylib>" "$<TARGET_FILE_DIR:main>/libraylib.so"
    )
endif()
