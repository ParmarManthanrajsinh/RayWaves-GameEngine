#include "DllLoader.h"
#include "Platform/PlatformModule.h"
#include "Platform/PlatformPaths.h"
#include <chrono>
#include <iostream>
#include <random>
#include <set>

namespace
{
    void DefaultLog(std::string_view message, bool is_error)
    {
        (is_error ? std::cerr : std::cout) << message << "\n";
    }

    DllLogSink &GetLogSink()
    {
        static DllLogSink sink;
        return sink;
    }

    void Log(std::string_view message, bool is_error)
    {
        DllLogSink &sink = GetLogSink();
        if (sink)
        {
            sink(message, is_error);
        }
        else
        {
            DefaultLog(message, is_error);
        }
    }
} // namespace

std::string GetHostExePath()
{
    return platform::HostExecutablePath().string();
}

void SetDllLogSink(DllLogSink sink) { GetLogSink() = std::move(sink); }

std::string FormatAbiMismatchMessage(bool has_version_export,
                                     uint32_t dll_version,
                                     uint32_t expected_version)
{
    if (!has_version_export)
    {
        return "GameLogic DLL is outdated (missing GetGameLogicAbiVersion "
               "export;"
               " it was built with an older engine version)."
               " Rebuild GameLogic (Compile in the editor) with the current "
               "engine version.";
    }
    return "GameLogic ABI mismatch: DLL reports version " +
           std::to_string(dll_version) + ", engine expects version " +
           std::to_string(expected_version) +
           "."
           " Rebuild GameLogic (Compile in the editor) with the current engine "
           "version.";
}

void CleanupStaleShadowCopies()
{
    try
    {
        fs::path temp_dirs[] = {fs::current_path() / ".raywaves" / "shadows",
                                fs::temp_directory_path()};
        auto now             = fs::file_time_type::clock::now();
        int cleaned          = 0;

        for (const auto &temp_dir : temp_dirs)
        {
            if (!fs::exists(temp_dir))
                continue;

            for (const auto &entry : fs::directory_iterator(temp_dir))
            {
                if (!entry.is_regular_file())
                    continue;

                std::string filename = entry.path().filename().string();
                if (!filename.contains(".shadow."))
                    continue;
                if (entry.path().extension() !=
                    platform::SharedLibrarySuffix())
                    continue;

                // Only delete files older than 1 hour
                std::error_code ec;
                auto age = now - entry.last_write_time(ec);
                if (ec)
                    continue;

                if (age > std::chrono::hours(1))
                {
                    fs::remove(entry.path(), ec);
                    if (!ec)
                        ++cleaned;
                }
            }
        }

        if (cleaned > 0)
        {
            Log("Cleaned up " + std::to_string(cleaned) +
                    " stale shadow DLL copies.",
                false);
        }
    }
    catch (std::exception const &e)
    {
        Log(std::string("Shadow cleanup error: ") + e.what(), true);
    }
    catch (...)
    {
        Log("Shadow cleanup: unknown error.", true);
    }
}

DllHandle LoadDll(const char *PATH)
{
    /*
      dlopen maps the module MAP_PRIVATE, and a mapped file cannot be
      truncated in place (the compiler gets ETXTBSY), so the module on disk
      cannot be rewritten while the host runs. Copy it to a temporary uniquely
      named file (shadow copy) and load that instead.
    */

    DllHandle result{nullptr, {}};

    try
    {
        fs::path src_path = fs::path(PATH);
        if (!fs::exists(src_path))
        {
            // Fall back to trying to load directly (will fail similarly if
            // missing)
            result.handle      = platform::ModuleLoad(PATH);
            result.shadow_path = PATH;
            return result;
        }

        // Determine destination directory: use local .raywaves/shadows if it
        // exists, otherwise fall back to the system temporary directory.
        fs::path temp_dir = fs::current_path() / ".raywaves" / "shadows";
        if (!fs::exists(temp_dir))
        {
            temp_dir = fs::temp_directory_path();
        }

        // Use random subdirectory to prevent symlink attacks
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
        std::string rand_dir = std::to_string(dis(gen));
        temp_dir             = temp_dir / rand_dir;
        std::error_code ec_dir;
        fs::create_directories(temp_dir, ec_dir);

        // Build a unique filename: GameLogic.shadow.<pid>.<tick>.so
        auto pid = platform::CurrentProcessId();
        auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

        // stem() will return file name without extention
        std::string base_name   = src_path.stem().string();
        std::string unique_name = base_name + ".shadow." + std::to_string(pid) +
                                  "." + std::to_string(ticks) +
                                  src_path.extension().string();

        fs::path dest_path = temp_dir / unique_name;

        // Copy to destination (overwrite not expected due to uniqueness)
        fs::copy_file(src_path, dest_path,
                      fs::copy_options::overwrite_existing);

        // dlopen is unreliable with relative paths.
        result.handle =
            platform::ModuleLoad(fs::absolute(dest_path).string().c_str());
        result.shadow_path = dest_path.string();
        return result;
    }
    catch (std::exception const &e)
    {
        Log(std::string("Shadow copy failed: ") + e.what() +
                ". Falling back to direct load.",
            true);
        result.handle      = platform::ModuleLoad(PATH);
        result.shadow_path = PATH;
        return result;
    }
    catch (...)
    {
        Log("Shadow copy: unknown error. Falling back to direct load.", true);
        result.handle      = platform::ModuleLoad(PATH);
        result.shadow_path = PATH;
        return result;
    }
}

void UnloadDll(DllHandle &dll)
{
    if (dll.handle != nullptr)
    {
        platform::ModuleUnload(dll.handle);
        dll.handle = nullptr;
    }

    // Delete the shadow copy after unloading. dlclose releases the mapping
    // synchronously, so one attempt is enough; the 1-hour stale sweep is the
    // final backstop if something still holds the file.
    if (!dll.shadow_path.empty())
    {
        fs::path p = fs::path(dll.shadow_path);

        // Only delete if it looks like one of our shadow copies
        std::string filename = p.filename().string();
        if (filename.contains(".shadow."))
        {
            std::error_code ec;
            fs::remove(p, ec);
            if (ec)
            {
                Log("Could not delete shadow module copy (will be swept later): " +
                        p.string(),
                    true);
            }
        }
        dll.shadow_path.clear();
    }
}

void *GetDllSymbol(const DllHandle &dll, const char *SYMBOL_NAME)
{
    if (dll.handle == nullptr)
    {
        return nullptr;
    }
    return platform::ModuleSymbol(dll.handle, SYMBOL_NAME);
}
