#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

struct DllHandle
{
    void *handle;
    // Absolute path of the shadow-copied module actually loaded via the
// platform loader (dlopen on Linux).
    // This allows unloading and deleting the copy so the original DLL remains
    // writable for recompilation while the application is running.
    std::string shadow_path;
};

DllHandle LoadDll(const char *path);
void UnloadDll(DllHandle &dll);
void *GetDllSymbol(const DllHandle &dll, const char *SYMBOL_NAME);

// Diagnostic sink for load/unload messages. Defaults to stderr, which suits
// the console hosts (tests, smoketest, standalone runtime). The GUI editor
// installs a sink that forwards into its terminal panel, since its stderr
// is invisible. Pass an empty sink to restore the stderr default.
using DllLogSink = std::function<void(std::string_view message, bool is_error)>;
void SetDllLogSink(DllLogSink sink);

// Human-readable rejection reason for a GameLogic DLL whose ABI version
// export is missing (pre-ABI DLL) or reports a different version than the
// engine expects. Pure function so it is unit-testable without loading.
std::string FormatAbiMismatchMessage(bool has_version_export,
                                     uint32_t dll_version,
                                     uint32_t expected_version);

// Sweep stale .shadow. module copies from %TEMP% left behind by crashes.
// Call once at engine startup.
void CleanupStaleShadowCopies();

// Absolute path of the running host executable. Backs the editor's
// application-directory lookup and the test raylib probes.
std::string GetHostExePath();
