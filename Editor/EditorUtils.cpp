#include "EditorUtils.h"
#include <iostream>
#include <windows.h>
#include <shellapi.h>

namespace EditorUtils
{
    void OpenInExplorer(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
        {
            ShellExecuteA(nullptr, "open", path.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else
        {
            std::cerr << "Failed to open path: directory does not exist. Path: " << path.string() << std::endl;
        }
    }
}
