# Host-side support shared by the editor, runtime, rwcli, and tests.
# No ImGui, no editor panels: only module loading, process spawning, and the
# cross-process agent lock. Implementations are selected per host platform.
add_library(RuntimeSupport STATIC
    Runtime/AgentLock.cpp
    Runtime/DllLoaderCommon.cpp
    Runtime/GameLogicHost.cpp
    ${RAYWAVES_PLATFORM_DIR}/DllLoader.cpp
    ${RAYWAVES_PLATFORM_DIR}/PeCrtCheck.cpp
    ${RAYWAVES_PLATFORM_DIR}/ProcessRunner.cpp
)

target_include_directories(RuntimeSupport PUBLIC
    ${CMAKE_SOURCE_DIR}/Runtime
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(RuntimeSupport PUBLIC Engine)
if(UNIX)
    target_link_libraries(RuntimeSupport PRIVATE ${CMAKE_DL_LIBS})
endif()
