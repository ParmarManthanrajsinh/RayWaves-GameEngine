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
raywaves_set_origin_rpath(GameLogic)

target_include_directories(GameLogic PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/templates/SlimeQuest/GameLogic
)
target_link_libraries(GameLogic PRIVATE Engine Threads::Threads)

add_dependencies(GameLogic stage_raylib)

# Compile-check the header-only rw templates so they cannot rot. They build no
# extra DLL; the real project build is covered by ProjectManager tests and by
# creating a project from the template in the editor.
foreach(TEMPLATE Simple2D Platformer2D Empty2D)
    add_library(${TEMPLATE}TemplateCheck OBJECT templates/${TEMPLATE}/GameLogic/Game.cpp)
    target_include_directories(${TEMPLATE}TemplateCheck PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Engine
        ${CMAKE_SOURCE_DIR}/templates/${TEMPLATE}/GameLogic
    )
    target_link_libraries(${TEMPLATE}TemplateCheck PRIVATE Engine)
    add_dependencies(${TEMPLATE}TemplateCheck stage_raylib)
endforeach()

# 3D template has no Game.cpp; RootManager.cpp owns the RW_GAME export.
add_library(ThirdPerson3DTemplateCheck OBJECT templates/ThirdPerson3D/GameLogic/RootManager.cpp)
target_include_directories(ThirdPerson3DTemplateCheck PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/templates/ThirdPerson3D/GameLogic
)
target_link_libraries(ThirdPerson3DTemplateCheck PRIVATE Engine)
add_dependencies(ThirdPerson3DTemplateCheck stage_raylib)

# Keep the built library next to the editor/runtime executables.
add_custom_command(TARGET GameLogic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:GameLogic>
            ${CMAKE_BINARY_DIR})
