#pragma once

#include <filesystem>
#include <string_view>

// Internal helpers shared by the Win32 and POSIX module loaders. Not part of
// the public DllLoader API.
namespace DllLoaderInternal
{
void Log(std::string_view message, bool is_error);

// Unique path under .raywaves/shadows or the system temp directory.
std::filesystem::path MakeShadowPath(const std::filesystem::path& source);

// Remove a shadow copy created by MakeShadowPath, retrying briefly on POSIX
// and Windows where the loader can release the file after dlclose/FreeLibrary.
void RemoveShadowFile(const std::filesystem::path& path);
} // namespace DllLoaderInternal
