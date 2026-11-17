# Host-side support shared by the editor, runtime, rwcli, and tests.
# No ImGui, no editor panels: only DLL loading, process spawning, and the
# cross-process agent lock.
add_library(RuntimeSupport STATIC
    Runtime/AgentLock.cpp
    Runtime/DllLoader.cpp
    Runtime/GameLogicHost.cpp
    Runtime/PeCrtCheck.cpp
    Runtime/PlatformInfo.cpp
    Runtime/ProcessRunner.cpp
)

target_include_directories(RuntimeSupport PUBLIC
    ${CMAKE_SOURCE_DIR}/Runtime
    ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(RuntimeSupport PUBLIC Engine)
