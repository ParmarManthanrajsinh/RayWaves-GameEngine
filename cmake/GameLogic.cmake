if(NOT RAYWAVES_BUILD_GAMELOGIC)
    return()
endif()

set(GAMELOGIC_SRC_CPP
    templates/SlimeQuest/GameLogic/DemoLevel.cpp
    templates/SlimeQuest/GameLogic/DemoMainMenu.cpp
    templates/SlimeQuest/GameLogic/GameCamera.cpp
    templates/SlimeQuest/GameLogic/Player.cpp
    templates/SlimeQuest/GameLogic/RootManager.cpp
    templates/SlimeQuest/GameLogic/Slime.cpp
)

add_library(GameLogic SHARED ${GAMELOGIC_SRC_CPP})
set_target_properties(GameLogic PROPERTIES PREFIX "")

target_include_directories(GameLogic PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/templates/SlimeQuest/GameLogic
)
target_link_libraries(GameLogic PRIVATE Engine)

add_dependencies(GameLogic stage_raylib)

# Keep the built DLL next to the editor/runtime executables.
add_custom_command(TARGET GameLogic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            ${CMAKE_BINARY_DIR})
