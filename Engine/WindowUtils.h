#pragma once

// Native window tweak hooks. Linux needs no per-window decoration work, so
// these are no-op stubs; the signatures stay to keep GameEngine call sites
// unchanged. Handles stay plain void* so no platform headers leak here.
namespace WindowUtils
{
    void ApplyDarkTitleBar(void* window_handle);
    void SetIconFromExecutable(void* window_handle);

    // Applies both tweaks to the current raylib window. Returns false when
    // there is no window handle (e.g. headless test hosts).
    bool SetupNativeWindow();
}
