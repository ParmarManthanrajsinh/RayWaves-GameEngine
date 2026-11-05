include(FetchContent)

# Always build raylib from source as a SHARED library so EXE and GameLogic share the same RLGL state
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Raylib examples to speed up build" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable Raylib tests" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP True
)
set(CMAKE_C_FLAGS_BACKUP "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -Wno-unused-command-line-argument")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-unused-command-line-argument")
FetchContent_MakeAvailable(raylib)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BACKUP}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")
unset(CMAKE_C_FLAGS_BACKUP)
unset(CMAKE_CXX_FLAGS_BACKUP)

# Vendored test framework, exposed as an interface target.
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE "${CMAKE_SOURCE_DIR}/third_party")

# Stage raylib headers + import library alongside the executables so generated
# per-project CMakeLists.txt can find them with exe-relative paths.
#
# NOTE: raylib is built *after* this configure step, so on a clean build
# directory the import library does not exist yet here. The stage_raylib
# target below is what guarantees the staged lib/ directory is actually
# populated; without it, project GameLogic builds fail with
# "-lraylib not found".
set(RAYLIB_BUILD_DIR "${CMAKE_BINARY_DIR}/_deps/raylib-build/raylib")
set(RAYLIB_STAGEDIR "${CMAKE_BINARY_DIR}/raylib")
file(MAKE_DIRECTORY "${RAYLIB_STAGEDIR}/include" "${RAYLIB_STAGEDIR}/lib" "${RAYLIB_STAGEDIR}/bin")

# Best-effort copy at configure time for build trees where raylib is already
# built (e.g. re-configuring an existing build directory).
foreach(HDR raylib.h raymath.h rcamera.h rlgl.h)
    if(EXISTS "${RAYLIB_BUILD_DIR}/include/${HDR}")
        file(COPY "${RAYLIB_BUILD_DIR}/include/${HDR}" DESTINATION "${RAYLIB_STAGEDIR}/include/")
    endif()
endforeach()
if(EXISTS "${RAYLIB_BUILD_DIR}/libraylib.dll.a")
    file(COPY "${RAYLIB_BUILD_DIR}/libraylib.dll.a" DESTINATION "${RAYLIB_STAGEDIR}/lib/")
    if(EXISTS "${RAYLIB_STAGEDIR}/lib/libraylib.dll.a" AND NOT EXISTS "${RAYLIB_STAGEDIR}/lib/libraylib.a")
        file(RENAME "${RAYLIB_STAGEDIR}/lib/libraylib.dll.a" "${RAYLIB_STAGEDIR}/lib/libraylib.a")
    endif()
endif()

# Stage again after every raylib build. This is required for clean build
# directories, where the configure-time copy above finds nothing.
#
# A custom target is used instead of add_custom_command(TARGET raylib ...)
# because raylib is created in the FetchContent subdirectory, and CMake
# rejects POST_BUILD hooks on targets created in another directory.
add_custom_target(stage_raylib
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${RAYLIB_STAGEDIR}/include" "${RAYLIB_STAGEDIR}/lib" "${RAYLIB_STAGEDIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_LINKER_FILE:raylib>" "${RAYLIB_STAGEDIR}/lib/libraylib.a"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:raylib>" "${RAYLIB_STAGEDIR}/bin/libraylib.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_BUILD_DIR}/include/raylib.h" "${RAYLIB_STAGEDIR}/include/raylib.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_BUILD_DIR}/include/raymath.h" "${RAYLIB_STAGEDIR}/include/raymath.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_BUILD_DIR}/include/rcamera.h" "${RAYLIB_STAGEDIR}/include/rcamera.h"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RAYLIB_BUILD_DIR}/include/rlgl.h" "${RAYLIB_STAGEDIR}/include/rlgl.h"
    COMMENT "Staging raylib for generated project builds"
    VERBATIM
)
add_dependencies(stage_raylib raylib)

# Copy runtime assets next to the executables.
if(EXISTS "${CMAKE_SOURCE_DIR}/EngineContent")
  file(COPY ${CMAKE_SOURCE_DIR}/EngineContent DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/EngineContent/config.ini")
  file(COPY ${CMAKE_SOURCE_DIR}/EngineContent/config.ini DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/templates/SlimeQuest/Assets")
  file(COPY ${CMAKE_SOURCE_DIR}/templates/SlimeQuest/Assets DESTINATION ${CMAKE_BINARY_DIR})
endif()
