#include "../Engine/MapManager.h"
#include "../Engine/GameMap.h"
#include "EmptyMap.h"
#include <cstdint>

static MapManager* s_GameMapManager = nullptr;

extern "C" __declspec(dllexport) uint32_t GetGameLogicAbiVersion()
{
    return RAYWAVES_GAMELOGIC_ABI_VERSION;
}

extern "C" __declspec(dllexport) GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<EmptyMap>("EmptyMap");
    }

    s_GameMapManager->b_GotoMap("EmptyMap");
    return s_GameMapManager;
}

extern "C" __declspec(dllexport) void DestroyGameMap(GameMap* map_manager)
{
    if (map_manager != nullptr)
    {
        delete map_manager;
        if (map_manager == s_GameMapManager)
        {
            s_GameMapManager = nullptr;
        }
    }
}
