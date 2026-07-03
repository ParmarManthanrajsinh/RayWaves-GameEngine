#include <iostream>
#include "../Engine/MapManager.h"
#include <raylib.h>
class EmptyMap : public GameMap
{
public:
    void Initialize() override 
    {
        std::cout << "EmptyMap initialized\n";
    }

    void Update(float delta_time) override 
    {
    }

    void Draw() override 
    {
        ClearBackground(RAYWHITE);
        DrawText("Empty Project", 10, 10, 20, DARKGRAY);
    }
};

static MapManager* s_GameMapManager = nullptr;

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
    if (map_manager)
    {
        delete map_manager;
        if (map_manager == s_GameMapManager)
        {
            s_GameMapManager = nullptr;
        }
    }
}

