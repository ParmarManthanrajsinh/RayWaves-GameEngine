#pragma once

// Module loading seam over dlfcn.h so Game/DllLoader.cpp keeps its shadow-copy
// logic free of loader specifics.

#include <dlfcn.h>
#include <unistd.h>

namespace platform
{
using ModuleHandle = void *;

// RTLD_NOW: a missing libraylib.so fails at load with a real error string
// instead of at the first dlsym. RTLD_LOCAL: plugin symbols must not leak
// into the global namespace and collide with the host's Engine copy.
inline ModuleHandle ModuleLoad(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

inline void *ModuleSymbol(ModuleHandle handle, const char *name)
{
    return dlsym(handle, name);
}

inline void ModuleUnload(ModuleHandle handle)
{
    if (handle)
    {
        dlclose(handle);
    }
}

inline unsigned long CurrentProcessId()
{
    return static_cast<unsigned long>(::getpid());
}

} // namespace platform
