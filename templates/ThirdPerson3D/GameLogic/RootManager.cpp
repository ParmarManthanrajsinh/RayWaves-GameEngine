#include "../Engine/GameLogicExports.h"
#include "../Engine/GameMap.h"
#include "../Engine/MapManager.h"
#include "ThirdPersonMap.h"
#include <cstdint>

static MapManager* s_GameMapManager = nullptr;

RW_GAMELOGIC_EXPORT uint32_t GetGameLogicAbiVersion()
{
    return RAYWAVES_GAMELOGIC_ABI_VERSION;
}

RW_GAMELOGIC_EXPORT GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<ThirdPersonMap>("ThirdPersonMap");
    }

    s_GameMapManager->b_GotoMap("ThirdPersonMap");
    return s_GameMapManager;
}

RW_GAMELOGIC_EXPORT void DestroyGameMap(GameMap* map_manager)
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
