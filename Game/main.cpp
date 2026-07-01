#include "GameEditor.h"
#include "GameEngine.h"
#include "GameMap.h"
#include <crtdbg.h>

#include "../Engine/ProjectManager.h"

// DLL loading now handled by GameEditor for hot-reload
int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--project")
    {
        ProjectManager::b_OpenProject(argv[2]);
    }

#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    std::println("Game Engine Starting...");
    GameEditor editor;
    editor.Init(1280,720,"RayWaves");
    
    // Load logic DLL if a project was opened from command line
    if (ProjectManager::b_HasOpenProject())
    {
        editor.b_LoadGameLogic(ProjectManager::GetCurrent().m_DllPath);
    }
    
    editor.Run();
    return 0;
}