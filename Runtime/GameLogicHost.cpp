#include "GameLogicHost.h"
#include "../Engine/GameEngine.h"
#include "../Engine/GameMap.h"
#include "../Engine/MapManager.h"

GameLogicHost::GameLogicHost(GameEngine& engine) : m_Engine(engine)
{
}

GameLogicHost::~GameLogicHost()
{
    Unload();
}

bool GameLogicHost::b_Load(std::string_view dll_path, std::string& out_error)
{
    out_error.clear();
    Unload();

    m_Dll = LoadDll(std::string(dll_path).c_str());
    if (m_Dll.handle == nullptr)
    {
        out_error = "Failed to load GameLogic DLL: " + std::string(dll_path);
        return false;
    }

    m_CreateGameMap = reinterpret_cast<CreateGameMapFunc>(GetDllSymbol(m_Dll, "CreateGameMap"));
    m_DestroyGameMap = reinterpret_cast<DestroyGameMapFunc>(GetDllSymbol(m_Dll, "DestroyGameMap"));

    if (m_CreateGameMap == nullptr || m_DestroyGameMap == nullptr)
    {
        out_error = "GameLogic DLL is missing CreateGameMap/DestroyGameMap";
        Unload();
        return false;
    }

    auto abi_version = reinterpret_cast<AbiVersionFunc>(GetDllSymbol(m_Dll, "GetGameLogicAbiVersion"));
    const bool b_has_version = (abi_version != nullptr);
    const uint32_t dll_version = b_has_version ? abi_version() : 0;
    if (!b_has_version || dll_version != RAYWAVES_GAMELOGIC_ABI_VERSION)
    {
        out_error = FormatAbiMismatchMessage(b_has_version, dll_version, RAYWAVES_GAMELOGIC_ABI_VERSION);
        Unload();
        return false;
    }

    m_Map = m_CreateGameMap();
    if (m_Map == nullptr)
    {
        out_error = "CreateGameMap returned null";
        Unload();
        return false;
    }

    if (m_Map->b_IsMapManager())
    {
        m_MapManager = static_cast<MapManager*>(m_Map);
        m_Engine.SetMapManager(m_MapManager);
    }
    else
    {
        m_Engine.SetMap(m_Map);
    }

    return true;
}

void GameLogicHost::Unload()
{
    // The engine must drop every pointer into DLL code before the module is
    // unloaded; otherwise the map destructor runs after its code is gone.
    m_Engine.SetMap(nullptr);
    m_Engine.SetMapManager(nullptr);

    if (m_Map != nullptr && m_DestroyGameMap != nullptr)
    {
        m_DestroyGameMap(m_Map);
    }

    m_Map = nullptr;
    m_MapManager = nullptr;
    m_CreateGameMap = nullptr;
    m_DestroyGameMap = nullptr;

    if (m_Dll.handle != nullptr)
    {
        UnloadDll(m_Dll);
        m_Dll = {};
    }
}
