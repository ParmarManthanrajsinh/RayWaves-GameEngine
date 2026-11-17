# Console host used by agents: project inspection, build, validation, scene
# editing, and headless runs. Deliberately does not link ImGui or Editor.
add_executable(rwcli
    Cli/Args.cpp
    Cli/CliCommon.cpp
    Cli/Commands.cpp
    Cli/RunWorker.cpp
    Cli/main.cpp
)

target_include_directories(rwcli PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Runtime
    ${CMAKE_SOURCE_DIR}/Cli
)

target_link_libraries(rwcli PRIVATE Engine RuntimeSupport)
add_dependencies(rwcli stage_raylib)

add_custom_command(TARGET rwcli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:rwcli>
)
