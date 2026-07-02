#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
#define WIN32_LEAN_AND_MEAN

struct DllHandle 
{
    void* handle;
    // Absolute path of the shadow-copied DLL actually loaded via LoadLibrary.
    // This allows unloading and deleting the copy so the original DLL remains
    // writable for recompilation while the application is running.
    std::string shadow_path;
};

DllHandle LoadDll(const char* path);
void UnloadDll(DllHandle dll);
void* GetDllSymbol(DllHandle dll, const char* SYMBOL_NAME);

// Sweep stale .shadow. DLL copies from %TEMP% left behind by crashes.
// Call once at engine startup.
void CleanupStaleShadowCopies();
