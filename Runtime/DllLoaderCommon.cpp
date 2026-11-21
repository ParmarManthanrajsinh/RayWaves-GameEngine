#include "DllLoader.h"
#include "DllLoaderInternal.h"
#include "PlatformInfo.h"
#include "PlatformNames.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace
{
DllLogSink& GetLogSink()
{
    static DllLogSink sink;
    return sink;
}

void DefaultLog(std::string_view message, bool is_error)
{
    (is_error ? std::cerr : std::cout) << message << "\n";
}
} // namespace

namespace DllLoaderInternal
{
void Log(std::string_view message, bool is_error)
{
    DllLogSink& sink = GetLogSink();
    if (sink)
    {
        sink(message, is_error);
    }
    else
    {
        DefaultLog(message, is_error);
    }
}

std::filesystem::path MakeShadowPath(const std::filesystem::path& source)
{
    std::error_code error;

    // Prefer the project-local shadow directory next to the module. An editor
    // launched from a different working directory must not drop shadow copies
    // into whatever directory happens to be CWD.
    std::filesystem::path temp_dir = source.parent_path() / ".raywaves" / "shadows";
    if (!std::filesystem::exists(temp_dir, error))
    {
        // Tests and hosts that load a module from a temporary directory keep
        // using the CWD-local shadow root.
        temp_dir = std::filesystem::current_path() / ".raywaves" / "shadows";
        if (!std::filesystem::exists(temp_dir, error))
        {
            temp_dir = std::filesystem::temp_directory_path(error);
            if (error)
            {
                temp_dir = std::filesystem::current_path();
            }
        }
    }

    static std::mt19937_64 generator(std::random_device{}());
    std::uniform_int_distribution<uint64_t> distribution;

    const std::string base_name = source.stem().string();
    const std::string unique_name = base_name + ".shadow." + std::to_string(PlatformInfo::GetCurrentProcessId()) + "." +
        std::to_string(PlatformInfo::GetMonotonicMilliseconds()) + "." + std::to_string(distribution(generator)) +
        source.extension().string();
    return temp_dir / unique_name;
}

void RemoveShadowFile(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    constexpr int MAX_DELETE_ATTEMPTS = 5;
    for (int attempt = 0; attempt < MAX_DELETE_ATTEMPTS; ++attempt)
    {
        std::filesystem::remove(path, error);
        if (!error)
        {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (error)
    {
        Log("Could not delete shadow module copy (will be swept later): " + path.string(), true);
    }
}
} // namespace DllLoaderInternal

void CleanupStaleShadowCopies(const std::filesystem::path& project_root)
{
    const std::filesystem::path shadow_dir = project_root / ".raywaves" / "shadows";
    std::error_code error;
    if (!std::filesystem::is_directory(shadow_dir, error))
    {
        return;
    }

    const std::string suffix = rw::platform::c_SharedLibrarySuffix;
    const auto now = std::filesystem::file_time_type::clock::now();
    int cleaned = 0;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(shadow_dir, error))
    {
        if (error || !entry.is_regular_file(error))
        {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        if (!filename.contains(".shadow.") || entry.path().extension() != suffix)
        {
            continue;
        }

        const auto age = now - entry.last_write_time(error);
        if (error || age <= std::chrono::hours(1))
        {
            continue;
        }

        std::filesystem::remove(entry.path(), error);
        if (!error)
        {
            ++cleaned;
        }
    }

    if (cleaned > 0)
    {
        DllLoaderInternal::Log("Cleaned up " + std::to_string(cleaned) + " stale project shadow module copies.", false);
    }
}

void SetDllLogSink(DllLogSink sink)
{
    GetLogSink() = std::move(sink);
}

std::string FormatAbiMismatchMessage(bool has_version_export, uint32_t dll_version, uint32_t expected_version)
{
    if (!has_version_export)
    {
        return "GameLogic module is outdated (missing GetGameLogicAbiVersion export;"
               " it was built with an older engine version)."
               " Rebuild GameLogic (Compile in the editor) with the current engine version.";
    }
    return "GameLogic ABI mismatch: module reports version " + std::to_string(dll_version) +
        ", engine expects version " + std::to_string(expected_version) +
        "."
        " Rebuild GameLogic (Compile in the editor) with the current engine version.";
}

std::string GetHostExePath()
{
    return PlatformInfo::GetExecutablePath().string();
}

void CleanupStaleShadowCopies()
{
    try
    {
        const std::filesystem::path local_shadow_dir = std::filesystem::current_path() / ".raywaves" / "shadows";
        const std::filesystem::path temp_root = std::filesystem::temp_directory_path();
        const std::filesystem::path temp_dirs[] = { local_shadow_dir, temp_root };
        const std::string suffix = rw::platform::c_SharedLibrarySuffix;
        auto now = std::filesystem::file_time_type::clock::now();
        int cleaned = 0;

        auto remove_if_stale = [&](const std::filesystem::directory_entry& entry)
        {
            std::error_code ec;
            if (!entry.is_regular_file(ec) || ec)
                return;

            const std::string filename = entry.path().filename().string();
            if (!filename.contains(".shadow."))
                return;
            if (entry.path().extension() != suffix)
                return;

            // Only delete files older than 1 hour.
            auto age = now - entry.last_write_time(ec);
            if (ec)
                return;

            if (age > std::chrono::hours(1))
            {
                std::filesystem::remove(entry.path(), ec);
                if (!ec)
                    ++cleaned;
            }
        };

        for (const auto& temp_dir : temp_dirs)
        {
            if (!std::filesystem::exists(temp_dir))
                continue;

            std::error_code ec;
            if (temp_dir == local_shadow_dir)
            {
                // Older builds wrote shadows into random subdirectories, so
                // scan recursively and then remove the empty leftovers.
                std::vector<std::filesystem::path> directories;
                for (std::filesystem::recursive_directory_iterator it(
                         temp_dir, std::filesystem::directory_options::skip_permission_denied, ec);
                     !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                {
                    if (it->is_directory(ec))
                    {
                        directories.push_back(it->path());
                    }
                    else
                    {
                        remove_if_stale(*it);
                    }
                }

                std::ranges::sort(directories, std::greater<>());
                for (const auto& directory : directories)
                {
                    std::error_code empty_ec;
                    if (std::filesystem::is_empty(directory, empty_ec) && !empty_ec)
                    {
                        std::filesystem::remove(directory, empty_ec);
                    }
                }
            }
            else
            {
                for (const auto& entry : std::filesystem::directory_iterator(temp_dir, ec))
                {
                    if (ec)
                        break;
                    remove_if_stale(entry);
                }
            }
        }

        if (cleaned > 0)
        {
            DllLoaderInternal::Log("Cleaned up " + std::to_string(cleaned) + " stale shadow module copies.", false);
        }
    }
    catch (std::exception const& e)
    {
        DllLoaderInternal::Log(std::string("Shadow cleanup error: ") + e.what(), true);
    }
    catch (...)
    {
        DllLoaderInternal::Log("Shadow cleanup: unknown error.", true);
    }
}
