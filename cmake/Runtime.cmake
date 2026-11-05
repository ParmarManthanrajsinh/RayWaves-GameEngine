add_executable(game WIN32
    Runtime/game.cpp
    Runtime/DllLoader.cpp
    Runtime/PeCrtCheck.cpp
    EngineContent/app.rc
)
add_executable(raywaves::runtime ALIAS game)
target_link_options(game PRIVATE -Wl,--subsystem,windows)

if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(game PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Runtime
)

target_link_libraries(game PRIVATE Engine)
add_dependencies(game stage_raylib)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:game> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:game>
)

if(RAYWAVES_BUILD_GAMELOGIC)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:GameLogic>
                $<TARGET_FILE_DIR:game>
    )
endif()
