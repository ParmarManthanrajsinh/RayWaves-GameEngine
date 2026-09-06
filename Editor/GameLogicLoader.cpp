#include "GameLogicLoader.h"
#include "../Engine/GameEngine.h"
#include "../Engine/MapManager.h"
#include "../Engine/GameState.h"
#include "../Engine/Profiler.h"
#include <system_error>

using Clock = std::chrono::steady_clock;

GameLogicLoader::GameLogicLoader(GameEngine& engine)
    : m_Engine(engine)
{
}

GameLogicLoader::~GameLogicLoader()
{
    Unload();
}

void GameLogicLoader::SetLogSink(LogSink sink)
{
    m_LogSink = std::move(sink);
    // Route the shared DllLoader diagnostics through the same sink so the
    // GUI editor shows them in its terminal instead of a hidden stderr.
    if (m_LogSink)
    {
        LogSink forward = m_LogSink;
        SetDllLogSink([forward](std::string_view message, bool is_error)
        {
            forward(message, is_error);
        });
    }
    else
    {
        SetDllLogSink(nullptr);
    }
}

void GameLogicLoader::SetNewMapCallback(NewMapCallback callback)
{
    m_NewMapCallback = std::move(callback);
}

void GameLogicLoader::SetGameLogicPath(std::string_view path)
{
    m_GameLogicPath = (path.data() != nullptr) ? std::string(path) : "";
}

void GameLogicLoader::Log(std::string_view message, bool is_error) const
{
    if (m_LogSink)
    {
        m_LogSink(message, is_error);
    }
}

void GameLogicLoader::RequestReload()
{
    m_bNeedsReload = true;
}

bool GameLogicLoader::PollReloadRequested()
{
    return m_bNeedsReload.exchange(false);
}

void GameLogicLoader::CheckForChanges()
{
    if (m_GameLogicPath.empty())
    {
        return;
    }

    const auto CURRENT_TIME = Clock::now();
    auto elapsed_time = std::chrono::duration<float>(CURRENT_TIME - m_LastReloadCheckTime).count();

    if (elapsed_time > 0.5f)
    {
        m_LastReloadCheckTime = CURRENT_TIME;
        std::error_code ec;

        const fs::path PATH(m_GameLogicPath);

        auto now_write = fs::last_write_time(PATH, ec);

        if (!ec && now_write != m_LastLogicWriteTime)
        {
            if (m_LastLogicWriteTime != fs::file_time_type{})
            {
                RequestReload();
            }
            m_LastLogicWriteTime = now_write;
        }
    }
}

void GameLogicLoader::Unload()
{
    /*
        Ensure any GameMap instance (potentially from the DLL) is destroyed
        BEFORE unloading the DLL, otherwise vtable/function code may be gone
        when the map's destructor runs.
    */
    if (m_GameLogicDll.handle != nullptr)
    {
        if ((m_DestroyGameMap != nullptr) && (m_MapManager != nullptr))
        {
            m_DestroyGameMap(m_MapManager);
        }
        UnloadDll(m_GameLogicDll);
        m_GameLogicDll = {};
        m_CreateGameMap = nullptr;
        m_DestroyGameMap = nullptr;
    }

    m_MapManager = nullptr;
    m_Engine.SetMap(nullptr);
    m_Engine.SetMapManager(nullptr);
}

void GameLogicLoader::AttachMap(GameMap* game_map)
{
    if (game_map != nullptr)
    {
        // Check if the loaded map is a MapManager via the DLL-safe type query
        if (game_map->b_IsMapManager())
        {
            auto* map_manager = static_cast<MapManager*>(game_map);
            // If it's a MapManager, set it using the dedicated method
            m_Engine.SetMapManager(map_manager);

            // Store reference for map selection UI
            m_MapManager = m_Engine.GetMapManager();
        }
        else
        {
            // Otherwise, use the regular SetMap method
            m_Engine.SetMap(game_map);
            m_MapManager = nullptr; // No MapManager available
        }
    }
    else
    {
        m_Engine.SetMap(nullptr);
        m_MapManager = nullptr;
    }
}

bool GameLogicLoader::b_LoadGameLogic(std::string_view dll_path)
{
    SetGameLogicPath(dll_path);

    DllHandle new_dll = LoadDll(m_GameLogicPath.c_str());
    if (new_dll.handle == nullptr)
    {
        Log("Failed to load GameLogic DLL: " + m_GameLogicPath, true);
        return false;
    }

    // 2) Get factory
    auto new_factory =
    reinterpret_cast<CreateGameMapFunc>
    (
        GetDllSymbol(new_dll, "CreateGameMap")
    );

    auto new_destroy =
        reinterpret_cast<DestroyGameMapFunc>
    (
        GetDllSymbol(new_dll, "DestroyGameMap")
    );

    if ((new_factory == nullptr) || (new_destroy == nullptr))
    {
        Log("Failed to get CreateGameMap/DestroyGameMap from DLL", true);
        UnloadDll(new_dll);
        return false;
    }

    // ABI version check: refuse mismatched DLLs loudly instead of risking
    // heap corruption across the DLL boundary (StateBag shares the CRT heap).
    auto abi_version_fn =
        reinterpret_cast<AbiVersionFunc>
    (
        GetDllSymbol(new_dll, "GetGameLogicAbiVersion")
    );
    const bool b_HasVersionExport = (abi_version_fn != nullptr);
    const uint32_t dll_version = b_HasVersionExport ? abi_version_fn() : 0;
    if (!b_HasVersionExport || (dll_version != RAYWAVES_GAMELOGIC_ABI_VERSION))
    {
        Log(FormatAbiMismatchMessage(b_HasVersionExport, dll_version, RAYWAVES_GAMELOGIC_ABI_VERSION), true);
        UnloadDll(new_dll);
        return false;
    }

    // 3) Create the new map before disturbing current state
    GameMap* new_map = new_factory();
    if (new_map == nullptr)
    {
        Log("CreateGameMap returned null", true);
        UnloadDll(new_dll);
        return false;
    }

    bool b_IsReload = (m_GameLogicDll.handle != nullptr);
    StateBag reload_state;

    if (b_IsReload && m_bPreserveStateOnReload)
    {
        try
        {
            if (m_Engine.GetMapManager() != nullptr)
            {
                m_Engine.GetMapManager()->SaveState(reload_state);
            }
            else if (m_Engine.GetMap() != nullptr)
            {
                m_Engine.GetMap()->SaveState(reload_state);
            }
        }
        catch (const std::exception& e)
        {
            Log(std::string("SaveState threw an exception: ") + e.what(), true);
        }
        catch (...)
        {
            Log("SaveState threw an unknown exception", true);
        }
    }

    // 4) Destroy current map to release old DLL code before unloading
    m_Engine.SetMap(nullptr);
    m_Engine.SetMapManager(nullptr);

    // 5) Unload old DLL (if any)
    if (m_GameLogicDll.handle != nullptr)
    {
        if ((m_DestroyGameMap != nullptr) && (m_MapManager != nullptr))
        {
            m_DestroyGameMap(m_MapManager);
        }
        UnloadDll(m_GameLogicDll);
        m_GameLogicDll = {};
        m_CreateGameMap = nullptr;
        m_DestroyGameMap = nullptr;
    }

    // 6) Swap in new DLL and map
    m_GameLogicDll = new_dll;
    m_CreateGameMap = new_factory;
    m_DestroyGameMap = new_destroy;

    AttachMap(new_map);

    if (m_NewMapCallback)
    {
        m_NewMapCallback(new_map);
    }

    // Update watched timestamp
    // (watch the original DLL path, not the shadow)
    std::error_code ec;
    m_LastLogicWriteTime = fs::last_write_time(fs::path(m_GameLogicPath), ec);

    if (b_IsReload && m_bPreserveStateOnReload)
    {
        try
        {
            if (m_Engine.GetMapManager() != nullptr)
            {
                m_Engine.GetMapManager()->LoadState(reload_state);
            }
            else if (m_Engine.GetMap() != nullptr)
            {
                m_Engine.GetMap()->LoadState(reload_state);
            }
        }
        catch (const std::exception& e)
        {
            Log(std::string("LoadState threw an exception: ") + e.what(), true);
        }
        catch (...)
        {
            Log("LoadState threw an unknown exception", true);
        }
    }

    return true;
}

bool GameLogicLoader::b_ReloadGameLogic()
{
    SCOPED_TIMER("dll_reload");
    if (m_GameLogicPath.empty())
    {
        return false;
    }

    return b_LoadGameLogic(m_GameLogicPath);
}
