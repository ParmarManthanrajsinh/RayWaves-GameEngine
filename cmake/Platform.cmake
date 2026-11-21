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

# Static engine code is linked into GameLogic.so on Linux; those objects must
# be position independent.
if(UNIX)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Keep build-tree RPATHs relative so copied/packaged binaries do not embed
    # the build machine's absolute _deps paths.
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    # glibc marks DSOs containing STB_GNU_UNIQUE symbols as NODELETE, which
    # prevents dlclose() from unmapping GameLogic. Normal weak symbols are
    # enough for this engine; disable GNU unique in shared-library builds.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>)
endif()

# Common compiler flags. Keep these to compilers CMake reports as Clang/GNU;
# MSVC is not used by the presets but should not see unsupported flags.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$")
        add_compile_options(-msse4.2)
    endif()
    add_compile_options(-fno-sanitize=all)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Qunused-arguments)
    endif()
endif()

add_compile_definitions(IMGUI_DISABLE_SSE)

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
