#include "DllLoader.h"
#include "DllLoaderInternal.h"
#include "PlatformNames.h"

#include <exception>
#include <string>
#include <vector>
#include <windows.h>

DllHandle LoadDll(const char* path)
{
    // On Windows, LoadLibrary locks the file on disk, which prevents
    // recompiling the module while it is loaded. Copy to a uniquely named
    // shadow file and load that instead.
    DllHandle result{ nullptr, {} };

    try
    {
        fs::path src_path = fs::path(path);
        if (!fs::exists(src_path))
        {
            HMODULE direct = LoadLibraryA(path);
            result.handle = reinterpret_cast<void*>(direct);
            result.shadow_path = path;
            return result;
        }

        // CRT compatibility guard: EXE and module must use the same CRT flavor
        // (dynamic vs static). A mismatch corrupts the heap the moment a
        // std::string/std::unordered_map (StateBag) crosses the boundary.
        {
            char exe_path[MAX_PATH];
            if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0)
            {
                std::vector<std::string> exe_imports = GetModuleCrtImports(exe_path);
                std::vector<std::string> dll_imports = GetModuleCrtImports(path);
                if (!b_CrtImportsCompatible(exe_imports, dll_imports))
                {
                    DllLoaderInternal::Log(std::string("CRT mismatch: host EXE and ") + path +
                            " link different CRT flavors (static vs dynamic)."
                            " Rebuild GameLogic with the same CRT (/MD).",
                        true);
                    return result;
                }
            }
        }

        const fs::path dest_path = DllLoaderInternal::MakeShadowPath(src_path);
        std::error_code copy_error;
        fs::create_directories(dest_path.parent_path(), copy_error);
        fs::copy_file(src_path, dest_path, fs::copy_options::overwrite_existing, copy_error);
        if (copy_error)
        {
            DllLoaderInternal::Log(
                "Shadow copy failed: " + copy_error.message() + ". Falling back to direct load.", true);
            HMODULE direct = LoadLibraryA(path);
            result.handle = reinterpret_cast<void*>(direct);
            result.shadow_path = path;
            return result;
        }

        HMODULE mod = LoadLibraryA(dest_path.string().c_str());
        if (mod == nullptr)
        {
            // A failed load must not leave a shadow copy behind.
            std::error_code remove_ec;
            fs::remove(dest_path, remove_ec);
            DllLoaderInternal::Log("Failed to load module: " + dest_path.string(), true);
            return result;
        }

        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = dest_path.string();
        return result;
    }
    catch (std::exception const& e)
    {
        DllLoaderInternal::Log(std::string("Shadow copy failed: ") + e.what() + ". Falling back to direct load.", true);
        HMODULE mod = LoadLibraryA(path);
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = path;
        return result;
    }
    catch (...)
    {
        DllLoaderInternal::Log("Shadow copy: unknown error. Falling back to direct load.", true);
        HMODULE mod = LoadLibraryA(path);
        result.handle = reinterpret_cast<void*>(mod);
        result.shadow_path = path;
        return result;
    }
}

void UnloadDll(DllHandle& dll)
{
    if (dll.handle != nullptr)
    {
        FreeLibrary(reinterpret_cast<HMODULE>(dll.handle));
        dll.handle = nullptr;
    }

    if (!dll.shadow_path.empty())
    {
        const fs::path path = fs::path(dll.shadow_path);
        if (path.filename().string().contains(".shadow."))
        {
            DllLoaderInternal::RemoveShadowFile(path);
        }
        dll.shadow_path.clear();
    }
}

void* GetDllSymbol(const DllHandle& dll, const char* symbol_name)
{
    if (dll.handle == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(dll.handle), symbol_name));
}
