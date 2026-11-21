#include "../Engine/AssetResolver.h"
#include "../Engine/PlatformInfo.h"
#include "../Engine/PlatformNames.h"
#include "DllLoader.h"
#include "GameConfig.h"
#include "GameEngine.h"
#include "GameLogicHost.h"
#include <cstdint>
#include <filesystem>
#include <iostream>

namespace
{
// Exported games are often launched from a file manager or desktop shortcut.
// Resolve data relative to the executable, never the caller's working
// directory, so GameLogic.so, Assets, and config.ini are always found.
std::filesystem::path ResolveNextToExecutable(const std::filesystem::path& name)
{
    const std::filesystem::path exe_dir = PlatformInfo::GetExecutablePath().parent_path();
    if (exe_dir.empty())
    {
        return name;
    }

    const std::filesystem::path candidate = exe_dir / name;
    if (std::filesystem::exists(candidate))
    {
        return candidate;
    }

    // A packaged runtime lives in Core/ while the shared config sits one
    // level up next to the editor and CLI.
    const std::filesystem::path parent_candidate = exe_dir.parent_path() / name;
    if (!exe_dir.parent_path().empty() && std::filesystem::exists(parent_candidate))
    {
        return parent_candidate;
    }

    return candidate;
}
} // namespace

int main()
{
    const std::filesystem::path exe_dir = PlatformInfo::GetExecutablePath().parent_path();
    CleanupStaleShadowCopies(exe_dir);
    std::cout << "Starting game runtime..." << "\n";

    // Load configuration
    GameConfig& config = GameConfig::GetInstance();
    config.m_bLoadFromFile(ResolveNextToExecutable("config.ini").string());

    // Set Asset Resolver for standalone game
    AssetResolver::SetProjectAssetPath(ResolveNextToExecutable("Assets").string());

    GameEngine engine;
    if (!engine.LaunchWindow(config.GetWindowConfig()))
    {
        return 1;
    }

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
    const std::filesystem::path module_path = ResolveNextToExecutable(rw::platform::c_GameLogicModuleName);
    if (host.b_Load(module_path.string(), load_error))
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
