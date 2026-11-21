#pragma once

#include <string>
#include <string_view>

// Compile-time platform names shared by the engine, editor, runtime, and CLI.
// Dynamic platform services (paths, processes, modules) live in PlatformInfo
// and the Runtime/Editor platform implementations.
namespace rw::platform
{
#if defined(_WIN32)
inline constexpr bool b_IsWindows = true;
inline constexpr const char* c_ExecutableSuffix = ".exe";
inline constexpr const char* c_SharedLibrarySuffix = ".dll";
inline constexpr const char* c_GameLogicModuleName = "GameLogic.dll";
inline constexpr const char* c_RaylibRuntimeName = "libraylib.dll";
inline constexpr char c_PathListSeparator = ';';
#else
inline constexpr bool b_IsWindows = false;
inline constexpr const char* c_ExecutableSuffix = "";
inline constexpr const char* c_SharedLibrarySuffix = ".so";
inline constexpr const char* c_GameLogicModuleName = "GameLogic.so";
inline constexpr const char* c_RaylibRuntimeName = "libraylib.so";
inline constexpr char c_PathListSeparator = ':';
#endif

inline std::string ExecutableName(std::string_view base)
{
    return std::string(base) + c_ExecutableSuffix;
}

inline std::string SharedLibraryName(std::string_view base)
{
    return std::string(base) + c_SharedLibrarySuffix;
}

} // namespace rw::platform
