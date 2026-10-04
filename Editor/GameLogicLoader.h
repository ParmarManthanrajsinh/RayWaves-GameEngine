#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "../Game/DllLoader.h"

class GameEngine;
class GameMap;
class MapManager;

// Owns the GameLogic DLL lifecycle for the editor: shadow-copy loading,
// factory resolution, ABI compatibility checks, state-preserving swaps,
// and file-timestamp change detection.
//
// GameEditor keeps only orchestration (pause/resume around reloads, wiring
// the log sink to its terminal). Panels talk to this class through the
// editor's forwarding methods, so no panel code changes when the loader
// internals evolve.
class GameLogicLoader
{
public:
    using LogSink = std::function<void(std::string_view message, bool is_error)>;
    using NewMapCallback = std::function<void(GameMap* new_map)>;

    explicit GameLogicLoader(GameEngine& engine);
    ~GameLogicLoader();

    GameLogicLoader(const GameLogicLoader&) = delete;
    GameLogicLoader& operator=(const GameLogicLoader&) = delete;

    // Load the DLL at dll_path, create its map, and attach it to the engine.
    // On reload (a DLL is already loaded) the previous map's state is saved
    // to a StateBag and restored into the new map when preservation is on.
    bool b_LoadGameLogic(std::string_view dll_path);

    // Reload the currently loaded DLL path. Does NOT pause playback;
    // the caller (GameEditor) owns play-state around this call.
    bool b_ReloadGameLogic();

    // Poll the original DLL file timestamp; requests a reload when it
    // changed. Call once per frame from the editor loop.
    void CheckForChanges();

    // Destroy the current map and unload the DLL. Safe to call with
    // nothing loaded. Also runs from the destructor.
    void Unload();

    // Flagged by the async compile-completion callback (may be off-thread).
    void RequestReload();
    // Drain the flag (editor thread). Returns true once per request.
    bool PollReloadRequested();

    void SetLogSink(LogSink sink);
    void SetNewMapCallback(NewMapCallback callback);
    void SetGameLogicPath(std::string_view path);
    const std::string& GetGameLogicPath() const { return m_GameLogicPath; }
    MapManager* GetMapManager() const { return m_MapManager; }

    // Attach an externally created map (e.g. not from the DLL) to the engine,
    // dispatching MapManager vs plain GameMap via the DLL-safe type query.
    void AttachMap(GameMap* game_map);

    bool m_bPreserveStateOnReload = true;

private:
    using CreateGameMapFunc = GameMap* (*)();
    using DestroyGameMapFunc = void (*)(GameMap*);
    using AbiVersionFunc = uint32_t (*)();

    void Log(std::string_view message, bool is_error) const;

    GameEngine& m_Engine;

    DllHandle m_GameLogicDll{};
    CreateGameMapFunc m_CreateGameMap = nullptr;
    DestroyGameMapFunc m_DestroyGameMap = nullptr;

    std::string m_GameLogicPath;
    fs::file_time_type m_LastLogicWriteTime{};
    std::atomic<bool> m_bNeedsReload = false;
    std::chrono::steady_clock::time_point m_LastReloadCheckTime = std::chrono::steady_clock::now();

    MapManager* m_MapManager = nullptr;

    LogSink m_LogSink;
    NewMapCallback m_NewMapCallback;
};
