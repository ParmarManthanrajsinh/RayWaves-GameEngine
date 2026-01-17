#include "GameEngine.h"
#include "DllLoader.h"
#include "GameConfig.h"
using CreateGameMapFunc = GameMap* (*)();

static std::unique_ptr<GameMap> s_fLoadGameLogic
(
    std::string_view dll_path, DllHandle& out_handle
)
{
    out_handle = LoadDll(dll_path.data());
    if (!out_handle.handle)
    {
        std::println
        (
            std::cerr, 
            "Fatal error: failed to load GameLogic DLL: {}", 
            dll_path
        );
        return nullptr;
    }

    auto CreateFn = reinterpret_cast<CreateGameMapFunc>
    (
        GetDllSymbol(out_handle, "CreateGameMap")
    );
    if (!CreateFn)
    {
        std::println
        (
            std::cerr, 
            "Failed to find symbol CreateGameMap in GameLogic DLL"
        );
        UnloadDll(out_handle);
        out_handle = {nullptr, {}};
        return nullptr;
    }

    GameMap* raw = CreateFn();
    if (!raw)
    {
        std::println("CreateGameMap returned null");
        UnloadDll(out_handle);
        out_handle = {nullptr, {}};
        return nullptr;
    }

    return std::unique_ptr<GameMap>(raw);
}

int main()
{
    std::println("Starting game runtime...");

    // Load configuration
    GameConfig& config = GameConfig::GetInstance();
    config.m_bLoadFromFile("config.ini");
    
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

    DllHandle game_logic_handle{nullptr, {}};
    auto map = s_fLoadGameLogic("GameLogic.dll", game_logic_handle);
    if (map)
    {
        engine.SetMap(std::move(map));
    }
    else
    {
        std::println(std::cerr, "Running without GameLogic ( no map Loaded ).");
    }

    while (!WindowShouldClose())
    {
        // Handle Alt+Enter for fullscreen toggle
        if (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))
        {
            engine.ToggleFullscreen();
        }
        
        float dt = GetFrameTime();
        engine.UpdateMap(dt);

        BeginDrawing();
        ClearBackground(BLACK);
        engine.DrawMap();
        EndDrawing();
    }

    UnloadDll(game_logic_handle);
    CloseWindow();
    return 0;
}
