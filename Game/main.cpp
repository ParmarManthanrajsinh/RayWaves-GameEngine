#include "GameEditor.h"
#include "GameEngine.h"
#include "GameMap.h"
#include <filesystem>
#include <iostream>

#include "../Engine/ProjectManager.h"

// DLL loading is owned by GameLogicLoader (via GameEditor) for hot-reload
int main(int argc, char **argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--project")
    {
        ProjectManager::b_OpenProject(argv[2]);
    }
    else if (argc >= 2 && std::string(argv[1]) != "--project")
    {
        std::filesystem::path input(argv[1]);
        std::filesystem::path folder;
        if (input.filename() == "project.raywaves" ||
            input.extension() == ".raywaves")
        {
            folder = input.parent_path();
        }
        else
        {
            folder = input;
        }
        if (std::filesystem::exists(folder / "project.raywaves"))
        {
            ProjectManager::b_OpenProject(folder.string());
        }
    }

    CleanupStaleShadowCopies();
    std::cout << "Game Engine Starting..." << "\n";
    GameEditor editor;
    editor.Init(1280, 720, "RayWaves");

    // Load logic DLL if a project was opened from command line
    if (ProjectManager::b_HasOpenProject())
    {
        editor.b_LoadGameLogic(ProjectManager::GetCurrent().m_DllPath);
    }

    editor.Run();
    return 0;
}
