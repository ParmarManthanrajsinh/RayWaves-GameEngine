#include "DllLoader.h"
#include <Windows.h>


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

        // Determine destination directory: use the system temporary directory
        // This ensures we can always write the shadow copy even if the game is 
        // deployed in a restricted directory like Program Files.
        fs::path temp_dir = fs::temp_directory_path();

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
    catch (...)
    {
        // As a last resort, try direct load
        HMODULE mod = LoadLibraryA(PATH);
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = PATH;
        return result;
    }
}

void UnloadDll(DllHandle dll) 
{
    if (dll.handle) 
    {
        FreeLibrary
        (
            reinterpret_cast<HMODULE>(dll.handle)
        );
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