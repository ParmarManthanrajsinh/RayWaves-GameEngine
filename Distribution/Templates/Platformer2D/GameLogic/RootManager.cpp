#include "../Engine/MapManager.h"
#include "../Engine/GameMap.h"
#include "PlatformerMap.h"
#include <cstdint>

static MapManager* s_GameMapManager = nullptr;

extern "C" uint32_t GetGameLogicAbiVersion()
{
    return RAYWAVES_GAMELOGIC_ABI_VERSION;
}

extern "C" GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<PlatformerMap>("PlatformerMap");
    }

    s_GameMapManager->b_GotoMap("PlatformerMap");
    return s_GameMapManager;
}

extern "C" void DestroyGameMap(GameMap* map_manager)
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
