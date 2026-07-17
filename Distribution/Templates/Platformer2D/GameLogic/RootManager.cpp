#include <iostream>
#include "../Engine/MapManager.h"
#include <raylib.h>
class PlatformerMap : public GameMap
{
private:
    Vector2 m_PlayerPos{ 100.0f, 100.0f };
    float m_Speed = 200.0f;

public:
    void Initialize() override 
    {
        std::cout << "PlatformerMap initialized\n";
    }

    void Update(float delta_time) override 
    {
        if (IsKeyDown(KEY_RIGHT)) m_PlayerPos.x += m_Speed * delta_time;
        if (IsKeyDown(KEY_LEFT)) m_PlayerPos.x -= m_Speed * delta_time;
        if (IsKeyDown(KEY_UP)) m_PlayerPos.y -= m_Speed * delta_time;
        if (IsKeyDown(KEY_DOWN)) m_PlayerPos.y += m_Speed * delta_time;
    }

    void Draw() override 
    {
        ClearBackground(SKYBLUE);
        DrawRectangleV(m_PlayerPos, {32, 32}, RED);
        DrawText("Platformer2D Template", 10, 10, 20, DARKGRAY);
    }
};

static MapManager* s_GameMapManager = nullptr;

extern "C" __declspec(dllexport) GameMap* CreateGameMap()
{
    if (s_GameMapManager == nullptr)
    {
        s_GameMapManager = new MapManager();
        s_GameMapManager->RegisterMap<PlatformerMap>("PlatformerMap");
    }

    s_GameMapManager->b_GotoMap("PlatformerMap");
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

