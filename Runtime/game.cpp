#include "../Engine/AssetResolver.h"
#include "DllLoader.h"
#include "GameConfig.h"
#include "GameEngine.h"
#include "GameLogicHost.h"
#include <cstdint>
#include <iostream>

int main()
{
    CleanupStaleShadowCopies();
    std::cout << "Starting game runtime..." << "\n";

    // Load configuration
    GameConfig& config = GameConfig::GetInstance();
    config.m_bLoadFromFile("config.ini");

    // Set Asset Resolver for standalone game
    AssetResolver::SetProjectAssetPath("Assets");

    GameEngine engine;
    engine.LaunchWindow(config.GetWindowConfig());

    // Set FPS based on vsync setting
    if (config.GetWindowConfig().b_Vsync)
    {
        SetTargetFPS(0); // Let vsync handle it
    }
    else
    {
        SetTargetFPS(config.GetWindowConfig().target_fps);
    }

    GameLogicHost host(engine);
    bool b_ExitRequested = false;

    std::string load_error;
    if (host.b_Load("GameLogic.dll", load_error))
    {
        // Standalone: GameLogic may capture the cursor. The editor never sets
        // this, so editing keeps a usable mouse.
        host.GetMap()->SetStandaloneMode(true);

        GameMap* raw_map = host.GetMap();
        raw_map->SetExitCallback([&b_ExitRequested]() { b_ExitRequested = true; });
    }
    else
    {
        std::cerr << load_error << "\n";
        std::cerr << "Running without GameLogic ( no map Loaded )." << "\n";
    }

    while (!WindowShouldClose() && !b_ExitRequested)
    {
        // Handle Alt+Enter for fullscreen toggle
        if (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))
        {
            GameEngine::ToggleFullscreen();
        }

        float dt = GetFrameTime();
        engine.SetViewportSize(GetScreenWidth(), GetScreenHeight());
        engine.UpdateMap(dt);

        BeginDrawing();
        ClearBackground(BLACK);
        engine.DrawMap();
        EndDrawing();
    }

    host.Unload();

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
