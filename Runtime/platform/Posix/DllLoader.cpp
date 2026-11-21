#include "DllLoader.h"
#include "DllLoaderInternal.h"

#include <dlfcn.h>

#include <exception>
#include <string>

namespace
{
// Linux links GameLogic against the system libstdc++, shared with the host.
// glibc can therefore detach the plugin's exit handlers on dlclose.
constexpr int k_ModuleOpenFlags = RTLD_NOW | RTLD_LOCAL;
} // namespace

DllHandle LoadDll(const char* path)
{
    // POSIX can replace a mapped .so in place, but dlopen caches by path. A
    // unique shadow copy guarantees the next reload maps the freshly built
    // module instead of returning the already-loaded handle.
    DllHandle result{ nullptr, {} };

    try
    {
        fs::path src_path = fs::path(path);
        if (!fs::exists(src_path))
        {
            void* direct = dlopen(path, k_ModuleOpenFlags);
            result.handle = direct;
            result.shadow_path = path;
            if (direct == nullptr)
            {
                DllLoaderInternal::Log(std::string("dlopen failed: ") + dlerror(), true);
            }
            return result;
        }

        const fs::path dest_path = DllLoaderInternal::MakeShadowPath(src_path);
        std::error_code copy_error;
        fs::create_directories(dest_path.parent_path(), copy_error);
        fs::copy_file(src_path, dest_path, fs::copy_options::overwrite_existing, copy_error);
        if (copy_error)
        {
            DllLoaderInternal::Log(
                "Shadow copy failed: " + copy_error.message() + ". Falling back to direct load.", true);
            void* direct = dlopen(path, k_ModuleOpenFlags);
            result.handle = direct;
            result.shadow_path = path;
            if (direct == nullptr)
            {
                DllLoaderInternal::Log(std::string("dlopen failed: ") + dlerror(), true);
            }
            return result;
        }

        void* mod = dlopen(dest_path.string().c_str(), k_ModuleOpenFlags);
        if (mod == nullptr)
        {
            DllLoaderInternal::Log("Failed to load module " + dest_path.string() + ": " + std::string(dlerror()), true);
            std::error_code remove_ec;
            fs::remove(dest_path, remove_ec);
            return result;
        }

        result.handle = mod;
        result.shadow_path = dest_path.string();
        return result;
    }
    catch (std::exception const& e)
    {
        DllLoaderInternal::Log(std::string("Shadow copy failed: ") + e.what() + ". Falling back to direct load.", true);
        void* mod = dlopen(path, k_ModuleOpenFlags);
        result.handle = mod;
        result.shadow_path = path;
        if (mod == nullptr)
        {
            DllLoaderInternal::Log(std::string("dlopen failed: ") + dlerror(), true);
        }
        return result;
    }
    catch (...)
    {
        DllLoaderInternal::Log("Shadow copy: unknown error. Falling back to direct load.", true);
        void* mod = dlopen(path, k_ModuleOpenFlags);
        result.handle = mod;
        result.shadow_path = path;
        return result;
    }
}

void UnloadDll(DllHandle& dll)
{
    if (dll.handle != nullptr)
    {
        if (dlclose(dll.handle) != 0)
        {
            const char* error = dlerror();
            DllLoaderInternal::Log(
                std::string("dlclose failed: ") + (error != nullptr ? error : "unknown error"), true);
        }
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
    return dlsym(dll.handle, symbol_name);
}
