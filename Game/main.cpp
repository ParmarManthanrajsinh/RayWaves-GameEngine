#include "GameEditor.h"
#include "GameEngine.h"
#include "GameMap.h"
#include <crtdbg.h>

// DLL loading now handled by GameEditor for hot-reload
int main()
{

#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    std::println("Game Engine Starting...");
    GameEditor editor;
    editor.Init(1280,720,"RayWaves");

    // Load logic DLL and create the map (will show default map if load fails)
    editor.b_LoadGameLogic("GameLogic.dll");
    editor.Run();
    return 0;
}