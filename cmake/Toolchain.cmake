# Toolchain selection. Included from the root CMakeLists.txt *before*
# project() so CMake detects the host compiler.
#
# Windows uses the pinned Zig toolchain. Linux uses the system GCC/G++ so the
# host and GameLogic.so share the system libstdc++ runtime; that shared runtime
# is what makes safe dlclose() of gameplay modules possible.

if(NOT RAYWAVES_TOOLCHAIN)
    set(RAYWAVES_TOOLCHAIN "auto" CACHE STRING "Compiler toolchain: auto, zig, system")
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(RAYWAVES_HOST_WINDOWS TRUE)
    set(RAYWAVES_HOST_POSIX FALSE)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(RAYWAVES_HOST_WINDOWS FALSE)
    set(RAYWAVES_HOST_POSIX TRUE)
else()
    # The "Posix" backend is Linux-specific (/proc/self/exe, .desktop, X11).
    # Fail fast instead of producing a binary that configures and then
    # misbehaves on macOS/BSD.
    message(FATAL_ERROR
        "RayWaves supports Windows and Linux only (host was ${CMAKE_HOST_SYSTEM_NAME}).")
endif()

if(RAYWAVES_TOOLCHAIN STREQUAL "auto")
    if(RAYWAVES_HOST_WINDOWS)
        set(RAYWAVES_TOOLCHAIN "zig")
    else()
        set(RAYWAVES_TOOLCHAIN "system")
    endif()
endif()

if(RAYWAVES_HOST_WINDOWS)
    if(NOT RAYWAVES_TOOLCHAIN STREQUAL "zig")
        message(FATAL_ERROR "Windows builds require RAYWAVES_TOOLCHAIN=zig.")
    endif()

    set(RAYWAVES_ZIG_CC "${CMAKE_CURRENT_LIST_DIR}/../Tools/zig-cc.bat")
    set(RAYWAVES_ZIG_CXX "${CMAKE_CURRENT_LIST_DIR}/../Tools/zig-cxx.bat")
    set(RAYWAVES_ZIG_BINARY "${CMAKE_CURRENT_LIST_DIR}/../Tools/zig/zig.exe")
    set(RAYWAVES_SETUP_TOOL "${CMAKE_CURRENT_LIST_DIR}/../Tools/setup_tools.ps1")

    # Fetch Zig before compiler detection. A system Zig on PATH is fine too;
    # the wrappers fall back to it.
    if(NOT EXISTS "${RAYWAVES_ZIG_BINARY}")
        find_program(RAYWAVES_SYSTEM_ZIG zig)
        if(NOT RAYWAVES_SYSTEM_ZIG)
            message(STATUS "Zig toolchain not found. Running ${RAYWAVES_SETUP_TOOL}...")
            execute_process(
                COMMAND powershell -ExecutionPolicy Bypass -File "${RAYWAVES_SETUP_TOOL}" -SkipRcEdit -SkipCMake -SkipNinja
                WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
                RESULT_VARIABLE RAYWAVES_ZIG_FETCH_RESULT
            )
            if(NOT RAYWAVES_ZIG_FETCH_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to download Zig. Run ${RAYWAVES_SETUP_TOOL} manually.")
            endif()
        endif()
    endif()

    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER "${RAYWAVES_ZIG_CC}" CACHE FILEPATH "C compiler" FORCE)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${RAYWAVES_ZIG_CXX}" CACHE FILEPATH "C++ compiler" FORCE)
    endif()
else()
    if(RAYWAVES_TOOLCHAIN STREQUAL "zig")
        message(FATAL_ERROR
            "Linux builds cannot use the Zig toolchain: Zig statically links libc++, "
            "which prevents safe dlclose() of GameLogic modules. Use RAYWAVES_TOOLCHAIN=system.")
    endif()

    set(RAYWAVES_SETUP_TOOL "${CMAKE_CURRENT_LIST_DIR}/../Tools/setup_tools.sh")

    if(CMAKE_CXX_COMPILER MATCHES "zig-cxx|zig-cc")
        message(FATAL_ERROR
            "This build directory was configured with the Zig toolchain. "
            "Linux now requires GCC/G++; remove it with scripts/clean.sh --preset <preset>.")
    endif()

    find_program(RAYWAVES_SYSTEM_GCC gcc)
    find_program(RAYWAVES_SYSTEM_GXX g++)
    if(NOT RAYWAVES_SYSTEM_GCC OR NOT RAYWAVES_SYSTEM_GXX)
        message(FATAL_ERROR
            "Linux builds require GCC and G++. Install them with: "
            "sudo apt install build-essential")
    endif()

    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER "${RAYWAVES_SYSTEM_GCC}" CACHE FILEPATH "C compiler" FORCE)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER "${RAYWAVES_SYSTEM_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
    endif()
endif()

# Ninja is the only generator. Prefer the fetched copy so a fresh checkout
# does not depend on the host PATH.
if(NOT CMAKE_MAKE_PROGRAM)
    if(RAYWAVES_HOST_WINDOWS)
        set(RAYWAVES_NINJA "${CMAKE_CURRENT_LIST_DIR}/../Tools/ninja/ninja.exe")
    else()
        set(RAYWAVES_NINJA "${CMAKE_CURRENT_LIST_DIR}/../Tools/ninja/ninja")
    endif()
    if(EXISTS "${RAYWAVES_NINJA}")
        set(CMAKE_MAKE_PROGRAM "${RAYWAVES_NINJA}" CACHE FILEPATH "Ninja executable" FORCE)
    else()
        find_program(RAYWAVES_SYSTEM_NINJA ninja)
        if(RAYWAVES_SYSTEM_NINJA)
            set(CMAKE_MAKE_PROGRAM "${RAYWAVES_SYSTEM_NINJA}" CACHE FILEPATH "Ninja executable" FORCE)
        else()
            message(STATUS "Ninja not found. Fetching it...")
            if(RAYWAVES_HOST_WINDOWS)
                execute_process(
                    COMMAND powershell -ExecutionPolicy Bypass -File "${RAYWAVES_SETUP_TOOL}" -SkipZig -SkipRcEdit -SkipCMake
                    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
                    RESULT_VARIABLE RAYWAVES_NINJA_FETCH_RESULT
                )
            else()
                execute_process(
                    COMMAND bash "${RAYWAVES_SETUP_TOOL}" --skip-zig --skip-rcedit --skip-cmake
                    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
                    RESULT_VARIABLE RAYWAVES_NINJA_FETCH_RESULT
                )
            endif()
            if(RAYWAVES_NINJA_FETCH_RESULT EQUAL 0 AND EXISTS "${RAYWAVES_NINJA}")
                set(CMAKE_MAKE_PROGRAM "${RAYWAVES_NINJA}" CACHE FILEPATH "Ninja executable" FORCE)
            else()
                message(WARNING "Ninja not found. Run ${RAYWAVES_SETUP_TOOL} or install ninja.")
            endif()
        endif()
    endif()
endif()
