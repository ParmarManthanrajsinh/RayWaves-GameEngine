#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "PeCrtCheck.h"

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
void UnloadDll(DllHandle& dll);
void* GetDllSymbol(const DllHandle& dll, const char* SYMBOL_NAME);

// Diagnostic sink for load/unload messages. Defaults to stderr, which suits
// the console hosts (tests, smoketest, standalone runtime). The GUI editor
// installs a sink that forwards into its terminal panel, since its stderr
// is invisible. Pass an empty sink to restore the stderr default.
using DllLogSink = std::function<void(std::string_view message, bool is_error)>;
void SetDllLogSink(DllLogSink sink);

// Human-readable rejection reason for a GameLogic DLL whose ABI version
// export is missing (pre-ABI DLL) or reports a different version than the
// engine expects. Pure function so it is unit-testable without loading.
std::string FormatAbiMismatchMessage(bool has_version_export, uint32_t dll_version, uint32_t expected_version);

// Sweep stale .shadow. DLL copies from %TEMP% left behind by crashes.
// Call once at engine startup.
void CleanupStaleShadowCopies();

// Absolute path of the running host executable (used for CRT import checks).
std::string GetHostExePath();

// (PE import-table CRT inspection lives in PeCrtCheck.h, included above,
// so existing GetModuleCrtImports/b_CrtImportsCompatible callers keep working.)
