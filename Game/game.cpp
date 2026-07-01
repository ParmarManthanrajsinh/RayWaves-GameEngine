#include "GameEngine.h"
#include "DllLoader.h"
#include "GameConfig.h"
using CreateGameMapFunc = GameMap* (*)();
using DestroyGameMapFunc = void (*)(GameMap*);

static DestroyGameMapFunc s_DestroyGameMap = nullptr;

static GameMap* s_fLoadGameLogic
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
    
    s_DestroyGameMap = reinterpret_cast<DestroyGameMapFunc>
    (
        GetDllSymbol(out_handle, "DestroyGameMap")
    );
    
    if (!CreateFn || !s_DestroyGameMap)
    {
        std::println
        (
            std::cerr, 
            "Failed to find symbol CreateGameMap/DestroyGameMap in GameLogic DLL"
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

    return raw;
}

int main()
{
    std::println("Starting game runtime...");

    // Load configuration
    GameConfig& config = GameConfig::GetInstance();
    config.m_bLoadFromFile("config.ini");
    
    GameEngine engine;
    engine.LaunchWindow(config.GetWindowConfig());
    
    Image icon = LoadImage("Assets/EngineContent/icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);
    
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

    if (s_DestroyGameMap && engine.GetMap())
    {
        s_DestroyGameMap(engine.GetMap());
    }
    
    // Clear the map pointer from engine since we just destroyed it
    engine.SetMap(nullptr);

    UnloadDll(game_logic_handle);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
