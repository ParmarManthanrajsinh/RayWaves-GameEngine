#include "WindowUtils.h"

#include <raylib.h>

#define Rectangle WinAPIRectangle
#define CloseWindow WinAPICloseWindow
#define ShowCursor WinAPIShowCursor
#include <windows.h>
#include <dwmapi.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace WindowUtils
{
    void ApplyDarkTitleBar(void* window_handle)
    {
        HWND hwnd = static_cast<HWND>(window_handle);
        if (hwnd == nullptr)
        {
            return;
        }

        BOOL value = TRUE;
        // Windows 10 (attribute 19)
        DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));

        // Windows 11 (attribute 20)
        DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value));
    }

    void SetIconFromExecutable(void* window_handle)
    {
        HWND hwnd = static_cast<HWND>(window_handle);
        if (hwnd == nullptr)
        {
            return;
        }

        // Extract and set icon from executable
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        HICON hIcon = ExtractIconA(GetModuleHandle(nullptr), exePath, 0);
        if (hIcon != nullptr && hIcon != (HICON)1)
        {
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }

    bool SetupNativeWindow()
    {
        void* window_handle = GetWindowHandle();
        if (window_handle == nullptr)
        {
            return false;
        }

        ApplyDarkTitleBar(window_handle);
        SetIconFromExecutable(window_handle);
        return true;
    }
}
