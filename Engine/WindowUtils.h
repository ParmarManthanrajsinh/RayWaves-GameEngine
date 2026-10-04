#pragma once

// Native window tweaks (dark title bar, executable icon). The Win32 headers
// stay inside WindowUtils.cpp so GameEngine and its consumers never see them.
// Takes plain void* handles to keep <windows.h> out of this header.
namespace WindowUtils
{
    void ApplyDarkTitleBar(void* window_handle);
    void SetIconFromExecutable(void* window_handle);

    // Applies both tweaks to the current raylib window. Returns false when
    // there is no window handle (e.g. headless test hosts).
    bool SetupNativeWindow();
}
