#include "GameEditor.h"
#include "GameEngine.h"
#include "GameMap.h"

// DLL loading now handled by GameEditor for hot-reload
int main()
{
    std::println("Game Engine Starting...");
    GameEditor editor;
    editor.Init(1280,720,"RayWaves");

    // Load logic DLL and create the map (will show default map if load fails)
    editor.b_LoadGameLogic("GameLogic.dll");
    editor.Run();
    return 0;
}