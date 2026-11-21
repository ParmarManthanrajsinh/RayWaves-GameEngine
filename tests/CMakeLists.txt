set(TEST_SUPPORT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/support/doctest_main.cpp
)

set(UNIT_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/AgentLock_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/AssetResolver_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/BuildDiagnostics_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/EditorAssets_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/EditorCamera3D_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameConfig_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameEditor_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameEngine_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameMap_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/InputReplay_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/MapManager_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/PanelRegistry_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/PlatformNames_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Profiler_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ProjectManager_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Project_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ReloadRoundtrip_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/rw_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Scene_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/StateBag_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Terminal_t.cpp
)

set(INTEGRATION_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/Cli_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/CliLsp_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/Pty_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/DllLoader_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ExportService_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ProcessRunner_t.cpp
)

function(raywaves_configure_test_target target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Engine
        ${CMAKE_SOURCE_DIR}/Editor
        ${CMAKE_SOURCE_DIR}/Runtime
    )
    target_link_libraries(${target} PRIVATE Engine RuntimeSupport doctest raywaves_terminal_core Threads::Threads)
    raywaves_set_origin_rpath(${target})
    add_dependencies(${target} stage_raylib)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:raylib>
                $<TARGET_FILE_DIR:${target}>
    )
    if(UNIX)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_SONAME_FILE:raylib>" "$<TARGET_FILE_DIR:${target}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_LINKER_FILE:raylib>" "$<TARGET_FILE_DIR:${target}>/libraylib.so"
        )
    endif()
endfunction()

add_executable(raywaves_tests_unit ${TEST_SUPPORT_SRC} ${UNIT_TEST_SRC}
    ${CMAKE_SOURCE_DIR}/Editor/EditorAssets.cpp)
raywaves_configure_test_target(raywaves_tests_unit)

add_executable(raywaves_tests_integration
    ${TEST_SUPPORT_SRC}
    ${INTEGRATION_TEST_SRC}
    ${CMAKE_SOURCE_DIR}/Editor/ExportService.cpp
    ${RAYWAVES_EDITOR_PLATFORM_DIR}/EditorUtils.cpp
    ${CMAKE_SOURCE_DIR}/Editor/GameLogicLoader.cpp
)
raywaves_configure_test_target(raywaves_tests_integration)
add_dependencies(raywaves_tests_integration rwcli)

add_executable(raywaves_smoke
    ${CMAKE_CURRENT_SOURCE_DIR}/smoke/HotReloadSmoke.cpp
)
raywaves_configure_test_target(raywaves_smoke)
# The smoke host does not call raylib directly, so the linker may drop the
# DT_NEEDED entry. Loading a shadow-copied GameLogic module from
# .raywaves/shadows then cannot resolve libraylib.so.600. Keep the dependency.
if(UNIX)
    target_link_options(raywaves_smoke PRIVATE -Wl,--no-as-needed)
endif()

if(RAYWAVES_BUILD_GAMELOGIC)
    add_dependencies(raywaves_smoke GameLogic)
endif()

enable_testing()

add_test(NAME unit COMMAND raywaves_tests_unit)
set_tests_properties(unit PROPERTIES
    LABELS "unit"
    TIMEOUT 120
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_test(NAME integration COMMAND raywaves_tests_integration)
set_tests_properties(integration PROPERTIES
    LABELS "integration"
    TIMEOUT 300
    RUN_SERIAL TRUE
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_test(NAME smoke COMMAND raywaves_smoke --iterations 3)
set_tests_properties(smoke PROPERTIES
    LABELS "smoke"
    TIMEOUT 300
    RUN_SERIAL TRUE
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
