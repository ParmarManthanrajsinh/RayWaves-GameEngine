#include "../Engine/AssetResolver.h"
#include "DllLoader.h"
#include "GameEditor.h"
#include "GameEngine.h"
#include "GameMap.h"
#include <filesystem>
#include <iostream>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "../Engine/ProjectManager.h"

// DLL loading is owned by GameLogicLoader (via GameEditor) for hot-reload
int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--project")
    {
        if (ProjectManager::b_OpenProject(argv[2]))
        {
            CleanupStaleShadowCopies(argv[2]);
        }
    }
    else if (argc >= 2 && std::string(argv[1]) != "--project")
    {
        std::filesystem::path input(argv[1]);
        std::filesystem::path folder;
        if (input.filename() == "project.raywaves" || input.extension() == ".raywaves")
        {
            folder = input.parent_path();
        }
        else
        {
            folder = input;
        }
        if (std::filesystem::exists(folder / "project.raywaves"))
        {
            if (ProjectManager::b_OpenProject(folder.string()))
            {
                CleanupStaleShadowCopies(folder);
            }
        }
    }

#if defined(_WIN32) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    CleanupStaleShadowCopies();
    std::cout << "Game Engine Starting..." << "\n";
    GameEditor editor;
    if (!editor.Init(1280, 720, "RayWaves"))
    {
        return 1;
    }

    // Load logic DLL if a project was opened from command line
    if (ProjectManager::b_HasOpenProject())
    {
        AssetResolver::SetProjectAssetPath(ProjectManager::GetCurrent().m_AssetPath);
        editor.b_LoadGameLogic(ProjectManager::GetCurrent().m_DllPath);
    }

    editor.Run();
    return 0;
}
