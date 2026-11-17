#pragma once
#include "DllLoader.h"
#include <cstdint>
#include <string>
#include <string_view>

class GameEngine;
class GameMap;
class MapManager;

// Loads a GameLogic DLL, checks its ABI, creates the map, and attaches it to
// the engine. Shared by the standalone runtime and rwcli so the failure
// messages and MapManager detection live in one place.
class GameLogicHost
{
public:
    explicit GameLogicHost(GameEngine& engine);
    ~GameLogicHost();

    GameLogicHost(const GameLogicHost&) = delete;
    GameLogicHost& operator=(const GameLogicHost&) = delete;

    bool b_Load(std::string_view dll_path, std::string& out_error);
    void Unload();

    GameMap* GetMap() const { return m_Map; }
    MapManager* GetMapManager() const { return m_MapManager; }
    bool b_IsLoaded() const { return m_Dll.handle != nullptr; }

private:
    using CreateGameMapFunc = GameMap* (*)();
    using DestroyGameMapFunc = void (*)(GameMap*);
    using AbiVersionFunc = uint32_t (*)();

    GameEngine& m_Engine;
    DllHandle m_Dll;
    CreateGameMapFunc m_CreateGameMap = nullptr;
    DestroyGameMapFunc m_DestroyGameMap = nullptr;
    GameMap* m_Map = nullptr;
    MapManager* m_MapManager = nullptr;
};
