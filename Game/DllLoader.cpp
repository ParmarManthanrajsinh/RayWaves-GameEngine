#include <iostream>
#include "DllLoader.h"
#include <Windows.h>
#include <chrono>
#include <random>
void CleanupStaleShadowCopies()
{
    try
    {
        fs::path temp_dirs[] = { 
            fs::current_path() / ".raywaves" / "shadows",
            fs::temp_directory_path() 
        };
        auto now = fs::file_time_type::clock::now();
        int cleaned = 0;

        for (const auto& temp_dir : temp_dirs)
        {
            if (!fs::exists(temp_dir)) continue;

            for (const auto& entry : fs::directory_iterator(temp_dir))
            {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                if (filename.find(".shadow.") == std::string::npos) continue;
                if (entry.path().extension() != ".dll") continue;

                // Only delete files older than 1 hour
                std::error_code ec;
                auto age = now - entry.last_write_time(ec);
                if (ec) continue;

                if (age > std::chrono::hours(1))
                {
                    fs::remove(entry.path(), ec);
                    if (!ec) ++cleaned;
                }
            }
        }

        if (cleaned > 0)
        {
            std::cout << "Cleaned up " << cleaned << " stale shadow DLL copies." << "\n";
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << "Shadow cleanup error: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Shadow cleanup: unknown error.\n";
    }
}

DllHandle LoadDll(const char* PATH) 
{
    /*
      On Windows, LoadLibrary locks the file on disk, which prevents recompiling the DLL while the application is running. To avoid this, copy the DLL to a temporary uniquely named file (shadow copy) and load that instead.  
    */

    DllHandle result{ nullptr, {} };

    try
    {
        fs::path src_path = fs::path(PATH);
        if (!fs::exists(src_path))
        {
            // Fall back to trying to load directly (will fail similarly if missing)
            HMODULE direct = LoadLibraryA(PATH);
            result.handle = reinterpret_cast<void*>(direct);
            result.shadow_path = PATH;
            return result;
        }

        // Determine destination directory: use local .raywaves/shadows if it exists,
        // otherwise fall back to the system temporary directory.
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
        temp_dir = temp_dir / rand_dir;
        std::error_code ec_dir;
        fs::create_directories(temp_dir, ec_dir);

        // Build a unique filename: GameLogic.shadow.<pid>.<tick>.dll
        DWORD pid = GetCurrentProcessId();
        DWORD ticks = static_cast<DWORD>(GetTickCount64());

        // stem() will return file name without extention
        std::string base_name = src_path.stem().string();
        std::string unique_name = base_name 
            + ".shadow." 
            + std::to_string(pid) 
            + "." 
            + std::to_string(ticks) 
            + src_path.extension().string();

        fs::path dest_path = temp_dir / unique_name;

        // Copy to destination (overwrite not expected due to uniqueness)
        fs::copy_file
        (
            src_path, 
            dest_path, 
            fs::copy_options::overwrite_existing
        );

        HMODULE mod = LoadLibraryA(dest_path.string().c_str());
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = dest_path.string();
        return result;
    }
    catch (std::exception const& e)
    {
        std::cerr << "Shadow copy failed: " << e.what() << ". Falling back to direct load.\n";
        HMODULE mod = LoadLibraryA(PATH);
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = PATH;
        return result;
    }
    catch (...)
    {
        std::cerr << "Shadow copy: unknown error. Falling back to direct load.\n";
        HMODULE mod = LoadLibraryA(PATH);
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = PATH;
        return result;
    }
}

void UnloadDll(DllHandle& dll) 
{
    if (dll.handle) 
    {
        FreeLibrary
        (
            reinterpret_cast<HMODULE>(dll.handle)
        );
        dll.handle = nullptr;
    }

    // Attempt to delete the shadow copy after unloading. Ignore failures.
    if (!dll.shadow_path.empty())
    {
        fs::path p = fs::path(dll.shadow_path);
        
        // Only delete if it looks like one of our shadow copies
        std::string filename = p.filename().string();
        if (filename.find(".shadow.") != std::string::npos)
        {
            std::error_code ec;
            fs::remove(p, ec);
        }
        dll.shadow_path.clear();
    }
}

void* GetDllSymbol(DllHandle dll, const char* SYMBOL_NAME) 
{
    if (!dll.handle)
    {
        return nullptr;
    }
    return reinterpret_cast<void*>
    (
        GetProcAddress
        (
            reinterpret_cast<HMODULE>(dll.handle),
            SYMBOL_NAME
        )
    );
}
