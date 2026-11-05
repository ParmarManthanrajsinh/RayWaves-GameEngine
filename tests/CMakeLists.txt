set(TEST_SUPPORT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/support/doctest_main.cpp
)

set(UNIT_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/AssetResolver_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameConfig_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameEditor_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/GameMap_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/MapManager_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/PanelRegistry_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Profiler_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ProjectManager_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/Project_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ReloadRoundtrip_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/StateBag_t.cpp
)

set(INTEGRATION_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/DllLoader_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ExportService_t.cpp
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
    target_link_libraries(${target} PRIVATE Engine doctest)
    add_dependencies(${target} stage_raylib)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:raylib>
                $<TARGET_FILE_DIR:${target}>
    )
endfunction()

add_executable(raywaves_tests_unit ${TEST_SUPPORT_SRC} ${UNIT_TEST_SRC})
raywaves_configure_test_target(raywaves_tests_unit)

add_executable(raywaves_tests_integration
    ${TEST_SUPPORT_SRC}
    ${INTEGRATION_TEST_SRC}
    ${CMAKE_SOURCE_DIR}/Runtime/DllLoader.cpp
    ${CMAKE_SOURCE_DIR}/Runtime/PeCrtCheck.cpp
    ${CMAKE_SOURCE_DIR}/Editor/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/Editor/EditorUtils.cpp
)
raywaves_configure_test_target(raywaves_tests_integration)

add_executable(raywaves_smoke
    ${CMAKE_CURRENT_SOURCE_DIR}/smoke/HotReloadSmoke.cpp
    ${CMAKE_SOURCE_DIR}/Runtime/DllLoader.cpp
    ${CMAKE_SOURCE_DIR}/Runtime/PeCrtCheck.cpp
)
raywaves_configure_test_target(raywaves_smoke)

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
