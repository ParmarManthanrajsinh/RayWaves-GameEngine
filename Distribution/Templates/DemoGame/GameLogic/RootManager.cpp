#include "../Engine/MapManager.h"
#include "DemoMainMenu.h"
#include "DemoLevel.h"
#include <memory>

// ==============================================================================
// ASSET RESOLUTION:
// When loading textures, sounds, or other files, do NOT use hardcoded paths!
// Include "AssetResolver.h" and wrap your paths with AssetResolver::Resolve():
// Example: LoadTexture(AssetResolver::Resolve("player.png").c_str());
// This ensures your paths work regardless of the active project or CWD.
// ==============================================================================

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

    // Automatically load the Main Menu when initialized
    s_GameMapManager->SetInitialMap("DemoMainMenu");

    return s_GameMapManager;
}

extern "C" __declspec(dllexport) void DestroyGameMap(GameMap* map_manager)
{
    // The pointer passed in is our s_GameMapManager (as GameMap*)
    // Deleting it will call the virtual destructor and clean up all maps & textures
    if (map_manager)
    {
        delete map_manager;
        if (map_manager == s_GameMapManager)
        {
            s_GameMapManager = nullptr;
        }
    }
}

