#include "../Engine/MapManager.h"
#include "DemoMainMenu.h"
#include "DemoLevel.h"
#include <memory>

// Global static instance to ensure consistency across editor and runtime
static MapManager* s_GameMapManager = nullptr;

extern "C" __declspec(dllexport) GameMap* CreateGameMap()
{
    // If we already have a manager, reuse it to maintain map registrations
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();

        // Register your game maps - this happens only once
        s_GameMapManager->RegisterMap<DemoLevel>("DemoLevel");
        s_GameMapManager->RegisterMap<DemoMainMenu>("DemoMainMenu");
    }

    // Automatically load the Main Menu
    s_GameMapManager->b_GotoMap("DemoMainMenu");

    return s_GameMapManager;
}