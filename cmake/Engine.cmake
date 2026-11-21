set(ENGINE_SRC_CPP
    Engine/AssetResolver.cpp
    Engine/BuildDiagnostics.cpp
    Engine/GameConfig.cpp
    Engine/GameEngine.cpp
    Engine/GameMap.cpp
    Engine/MapManager.cpp
    Engine/PlatformInfo.cpp
    Engine/Profiler.cpp
    Engine/Project.cpp
    Engine/ProjectManager.cpp
    Engine/raygui_impl.cpp
    Engine/Scene.cpp
    Engine/WindowUtils.cpp
    Engine/X11WindowType.cpp
)

add_library(Engine STATIC ${ENGINE_SRC_CPP})
add_library(raywaves::engine ALIAS Engine)

target_include_directories(Engine PUBLIC
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Runtime
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Engine PUBLIC raylib)
if(WIN32)
    target_link_libraries(Engine PRIVATE dwmapi)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Engine PRIVATE X11::X11)
endif()
