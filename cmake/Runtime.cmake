set(RAYWAVES_RUNTIME_RC "")
if(WIN32)
    set(RAYWAVES_RUNTIME_RC EngineContent/app.rc)
endif()

add_executable(game WIN32
    Runtime/game.cpp
    ${RAYWAVES_RUNTIME_RC}
)
add_executable(raywaves::runtime ALIAS game)
raywaves_set_origin_rpath(game)

if(WIN32)
    target_link_options(game PRIVATE -Wl,--subsystem,windows)
endif()

if(RAYWAVES_DISTRIBUTION_BUILD)
    target_compile_definitions(game PRIVATE RAYWAVES_PROFILER_DISABLED)
endif()

target_include_directories(game PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Runtime
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(game PRIVATE Engine RuntimeSupport Threads::Threads)
add_dependencies(game stage_raylib)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Tools/rcedit.exe")
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_SOURCE_DIR}/Tools/rcedit.exe $<TARGET_FILE:game> --set-icon ${CMAKE_SOURCE_DIR}/EngineContent/icon.ico
    )
endif()
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:game>
)
if(UNIX)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_SONAME_FILE:raylib>" "$<TARGET_FILE_DIR:game>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_LINKER_FILE:raylib>" "$<TARGET_FILE_DIR:game>/libraylib.so"
    )
endif()

if(RAYWAVES_BUILD_GAMELOGIC)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:GameLogic>
                $<TARGET_FILE_DIR:game>
    )
endif()
