set(ENGINE_SRC_CPP
    Engine/AssetResolver.cpp
    Engine/GameConfig.cpp
    Engine/GameEngine.cpp
    Engine/GameMap.cpp
    Engine/MapManager.cpp
    Engine/Profiler.cpp
    Engine/Project.cpp
    Engine/ProjectManager.cpp
    Engine/raygui_impl.cpp
    Engine/WindowUtils.cpp
)

add_library(Engine STATIC ${ENGINE_SRC_CPP})
add_library(raywaves::engine ALIAS Engine)

target_include_directories(Engine PUBLIC
    ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(Engine PUBLIC raylib)
target_link_libraries(Engine PRIVATE dwmapi)
