#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "PeCrtCheck.h"

namespace fs = std::filesystem;

struct DllHandle
{
    void* handle = nullptr;
    // Absolute path of the shadow-copied module actually loaded. This allows
    // unloading and deleting the copy so the original module remains writable
    // for recompilation while the application is running.
    std::string shadow_path;

    DllHandle() = default;
    DllHandle(void* h, std::string path) : handle(h), shadow_path(std::move(path)) {}

    DllHandle(const DllHandle&) = delete;
    DllHandle& operator=(const DllHandle&) = delete;

    DllHandle(DllHandle&& other) noexcept : handle(other.handle), shadow_path(std::move(other.shadow_path))
    {
        other.handle = nullptr;
    }

    DllHandle& operator=(DllHandle&& other) noexcept
    {
        if (this != &other)
        {
            handle = other.handle;
            shadow_path = std::move(other.shadow_path);
            other.handle = nullptr;
        }
        return *this;
    }
};

DllHandle LoadDll(const char* path);
void UnloadDll(DllHandle& dll);
void* GetDllSymbol(const DllHandle& dll, const char* symbol_name);

// Diagnostic sink for load/unload messages. Defaults to stderr, which suits
// the console hosts (tests, smoketest, standalone runtime). The GUI editor
// installs a sink that forwards into its terminal panel, since its stderr
// is invisible. Pass an empty sink to restore the stderr default.
using DllLogSink = std::function<void(std::string_view message, bool is_error)>;
void SetDllLogSink(DllLogSink sink);

// Human-readable rejection reason for a GameLogic module whose ABI version
// export is missing or reports a different version than the engine expects.
std::string FormatAbiMismatchMessage(bool has_version_export, uint32_t dll_version, uint32_t expected_version);

// Sweep stale .shadow module copies left behind by crashes.
// Call once at engine startup for the CWD/temp roots, and once when a project
// is opened for its project-local shadow directory.
void CleanupStaleShadowCopies();
void CleanupStaleShadowCopies(const std::filesystem::path& project_root);

// Absolute path of the running host executable (used for CRT import checks).
std::string GetHostExePath();
