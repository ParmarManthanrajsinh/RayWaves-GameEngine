#include "WindowUtils.h"

namespace WindowUtils
{
    // Titlebar colour is compositor-controlled on Linux (GNOME, KDE, Sway each
    // expose it through their own protocol); there is no per-window call.
    void ApplyDarkTitleBar(void* window_handle)
    {
        (void)window_handle;
    }

    // The editor already sets the window icon through raylib
    // (GameEditor.cpp SetWindowIcon(LoadImage(...))), which is portable, so
    // nothing else is needed here.
    void SetIconFromExecutable(void* window_handle)
    {
        (void)window_handle;
    }

    bool SetupNativeWindow()
    {
        // GameEngine returns early on false and the window would never run its
        // loop, so a no-op host must still report success.
        return true;
    }
}
