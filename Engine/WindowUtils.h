#pragma once

// Native window tweaks (dark title bar, executable icon, X11 window type).
// Platform headers stay inside the .cpp files so GameEngine and its consumers
// never see them. Takes plain void* handles to keep <windows.h> out of this
// header.
namespace WindowUtils
{
void ApplyDarkTitleBar(void* window_handle);
void SetIconFromExecutable(void* window_handle);

// Linux/X11: advertise _NET_WM_WINDOW_TYPE_NORMAL and explicit size bounds
// before the window is mapped. Tiling compositors such as Hyprland float
// untyped XWayland windows and windows with unusable min/max hints.
void SetNormalWindowType();

// Applies both tweaks to the current raylib window. Returns false when
// there is no window handle (e.g. headless test hosts).
bool SetupNativeWindow();
} // namespace WindowUtils
