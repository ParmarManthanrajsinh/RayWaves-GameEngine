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
            std::filesystem::path abs_path = std::filesystem::absolute(path);
            abs_path.make_preferred();
            HINSTANCE result = ShellExecuteW(nullptr, L"explore", abs_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if ((intptr_t)result > 32) return true;
            std::cerr << "Failed to open path: ShellExecuteW failed with error code: " << (intptr_t)result << " Path: " << abs_path.string() << std::endl;
            return false;
        }
        else
        {
            std::cerr << "Failed to open path: directory does not exist. Path: " << path.string() << std::endl;
            return false;
        }
    }
}
