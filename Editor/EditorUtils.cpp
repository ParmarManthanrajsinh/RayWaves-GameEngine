#include "EditorUtils.h"
#include <iostream>
#include <windows.h>
#include <shellapi.h>

namespace EditorUtils
{
    bool OpenInExplorer(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
        {
            HINSTANCE result = ShellExecuteA(nullptr, "open", path.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if ((intptr_t)result > 32) return true;
            std::cerr << "Failed to open path: ShellExecuteA failed. Path: " << path.string() << std::endl;
            return false;
        }
        else
        {
            std::cerr << "Failed to open path: directory does not exist. Path: " << path.string() << std::endl;
            return false;
        }
    }
}
