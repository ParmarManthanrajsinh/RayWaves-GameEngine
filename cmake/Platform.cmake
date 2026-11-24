# Platform facts shared by every target module. Included after project().

if(WIN32)
    set(RAYWAVES_PLATFORM "win32")
    set(RAYWAVES_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Runtime/platform/Win32")
    set(RAYWAVES_EDITOR_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Editor/platform/Win32")
    set(RAYWAVES_EXE_SUFFIX ".exe")
    set(RAYWAVES_SHARED_SUFFIX ".dll")
    set(RAYWAVES_IMPORT_SUFFIX ".lib")
    set(RAYWAVES_RAYLIB_RUNTIME_NAME "libraylib.dll")
    set(RAYWAVES_GAMELOGIC_NAME "GameLogic.dll")
    set(RAYWAVES_RUNTIME_NAME "game.exe")
else()
    set(RAYWAVES_PLATFORM "posix")
    set(RAYWAVES_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Runtime/platform/Posix")
    set(RAYWAVES_EDITOR_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/Editor/platform/Posix")
    set(RAYWAVES_EXE_SUFFIX "")
    set(RAYWAVES_SHARED_SUFFIX ".so")
    set(RAYWAVES_IMPORT_SUFFIX ".so")
    set(RAYWAVES_RAYLIB_RUNTIME_NAME "libraylib.so")
    set(RAYWAVES_GAMELOGIC_NAME "GameLogic.so")
    set(RAYWAVES_RUNTIME_NAME "game")
endif()

set(RAYWAVES_EXE_DIR "${CMAKE_BINARY_DIR}")
set(RAYWAVES_RAYLIB_STAGEDIR "${CMAKE_BINARY_DIR}/raylib")

# Opt-in compiler cache for local engine builds: ccache when present, sccache as
# the fallback. OFF by default so CI, packaging, and clean-checkout builds keep
# using the compiler directly. Enable with -DRAYWAVES_USE_CCACHE=ON.
#
# The launchers are plain variables set here, before the target modules are
# included, so every engine target picks them up. A script wrapper is refused:
# cache tools spawn the real compiler, and the Windows Zig wrappers are .bat.
option(RAYWAVES_USE_CCACHE "Route engine compiles through ccache/sccache when found" OFF)
if(RAYWAVES_USE_CCACHE)
    find_program(RAYWAVES_CCACHE_PROGRAM ccache)
    find_program(RAYWAVES_SCCACHE_PROGRAM sccache)

    if(RAYWAVES_CCACHE_PROGRAM)
        set(RAYWAVES_COMPILER_CACHE "${RAYWAVES_CCACHE_PROGRAM}")
    elseif(RAYWAVES_SCCACHE_PROGRAM)
        set(RAYWAVES_COMPILER_CACHE "${RAYWAVES_SCCACHE_PROGRAM}")
    endif()

    if(NOT RAYWAVES_COMPILER_CACHE)
        message(WARNING "RAYWAVES_USE_CCACHE=ON but neither ccache nor sccache was found; building without a cache.")
    else()
        # Windows wrappers can be upper- or lower-case; MATCHES is case-sensitive.
        string(TOLOWER "${CMAKE_CXX_COMPILER}" RAYWAVES_COMPILER_LOWER)
        if(RAYWAVES_COMPILER_LOWER MATCHES "\\.(bat|cmd)$")
            message(WARNING
                "RAYWAVES_USE_CCACHE=ON but ${CMAKE_CXX_COMPILER} is a wrapper script; "
                "building without a cache. Configure a cache against a real compiler instead.")
        elseif(NOT CMAKE_CXX_COMPILER_LAUNCHER)
            set(CMAKE_C_COMPILER_LAUNCHER "${RAYWAVES_COMPILER_CACHE}")
            set(CMAKE_CXX_COMPILER_LAUNCHER "${RAYWAVES_COMPILER_CACHE}")
            message(STATUS "Compiler cache: ${RAYWAVES_COMPILER_CACHE}")
        endif()
    endif()
endif()

# Sanitizer instrumentation for the whole build tree (engine, tests, editor,
# runtime, CLI, and the raylib fetched in cmake/Dependencies.cmake). Off by
# default. Linux-only: the Windows/Zig path forces -fno-sanitize=all above, so
# a Windows sanitizer build cannot work and is rejected explicitly.
# Enable with e.g.: cmake --preset zig-debug -DRAYWAVES_SANITIZE=address,undefined
set(RAYWAVES_SANITIZE "" CACHE STRING "Sanitizers to enable, comma separated (Linux only, e.g. address,undefined)")
if(RAYWAVES_SANITIZE)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "RAYWAVES_SANITIZE=${RAYWAVES_SANITIZE} is Linux-only. "
            "Windows builds force -fno-sanitize=all in cmake/Platform.cmake.")
    endif()
    message(STATUS "Sanitizers: ${RAYWAVES_SANITIZE}")
    # -fno-omit-frame-pointer keeps sanitizer/valgrind backtraces readable;
    # -g gives symbolized frames even in RelWithDebInfo-style builds.
    add_compile_options(-fsanitize=${RAYWAVES_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${RAYWAVES_SANITIZE})
endif()

# Static engine code is linked into GameLogic.so on Linux; those objects must
# be position independent.
if(UNIX)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Keep build-tree RPATHs relative so copied/packaged binaries do not embed
    # the build machine's absolute _deps paths.
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    # Use the target's INSTALL_RPATH in the build tree too. This drops CMake's
    # automatic _deps entries and leaves only $ORIGIN in packaged binaries.
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    # glibc marks DSOs containing STB_GNU_UNIQUE symbols as NODELETE, which
    # prevents dlclose() from unmapping GameLogic. Normal weak symbols are
    # enough for this engine; disable GNU unique in shared-library builds.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>)
endif()

# Common compiler flags. Keep these to compilers CMake reports as Clang/GNU;
# MSVC is not used by the presets but should not see unsupported flags.
#
# These three settings are Linux-hostile leftovers from the Windows/Zig
# toolchain: forcing SSE4.2 raises the CPU baseline, disabling sanitizers
# removes the most useful debug tool, and IMGUI_DISABLE_SSE strips ImGui's
# SIMD path on a platform where SSE2 is guaranteed. Keep them on Windows and
# leave Linux builds portable.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(WIN32)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
            add_compile_options(-msse4.2)
        endif()
        add_compile_options(-fno-sanitize=all)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Qunused-arguments)
    endif()
endif()

if(WIN32)
    add_compile_definitions(IMGUI_DISABLE_SSE)
endif()

if(WIN32)
    add_compile_definitions(RAYWAVES_PLATFORM_WINDOWS=1)
else()
    add_compile_definitions(RAYWAVES_PLATFORM_POSIX=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # WindowUtils sets _NET_WM_WINDOW_TYPE before mapping so tiling
    # compositors do not float the editor.
    find_package(X11 REQUIRED)
endif()

# Set $ORIGIN RPATHs so packaged executables and GameLogic.so find the
# libraylib.so staged next to them.
function(raywaves_set_origin_rpath target)
    if(UNIX)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endfunction()
