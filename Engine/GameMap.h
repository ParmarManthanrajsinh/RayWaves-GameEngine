#pragma once
#include "EditorEntity.h"
#include "GameState.h"
#include <cstdint>
#include <functional>
#include <raylib.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// GameLogic DLL ABI version. GameLogic DLLs must export
// GetGameLogicAbiVersion() returning this value. The editor/runtime reject
// DLLs with a different (or missing) version instead of risking heap
// corruption across the DLL boundary.
inline constexpr uint32_t RAYWAVES_GAMELOGIC_ABI_VERSION = 7;

class GameMap
{
protected:
    std::string m_MapName;
    float m_SceneWidth = 0.0f;
    float m_SceneHeight = 0.0f;
    int m_TargetFPS = 60;
    std::string m_ProjectAssetPath;
    bool m_bStandalone = false;

    // Live gameplay objects exposed to the editor. Cleared before every
    // Initialize/Reset; maps register after their entities exist.
    std::vector<t_EditorEntity> m_EditorEntities;
    uint64_t m_NextEditorEntityId = 1;

    // Host-owned editor camera. Only valid while the host is editing a 3D map.
    Camera3D m_EditorViewCamera3D{};
    bool m_bHasEditorViewCamera3D = false;

    // Transition callback to request a map change via the manager
    std::function<void(std::string_view, bool)> m_TransitionCallback;

    // Exit callback so DLL can request shutdown without calling CloseWindow() directly
    std::function<void()> m_ExitCallback;

public:
    GameMap();
    GameMap(std::string_view map_name);
    virtual ~GameMap() = default;

    virtual void Initialize();
    virtual void Update(float delta_time);
    virtual void Draw();

    virtual void SetProjectAssetPath(const std::string& path);

    virtual void SaveState(StateBag& out) const {}
    virtual void LoadState(const StateBag& in) {}

    // Type query across the DLL boundary. RTTI/typeid cannot be trusted
    // between separately linked modules, so MapManager identifies itself
    // via this virtual instead. Never guess by map name.
    virtual bool b_IsMapManager() const { return false; }

    void SetMapName(std::string_view map_name);
    std::string GetMapName() const;
    void SetSceneBounds(float width, float height);
    Vector2 GetSceneBounds() const;

    // Camera used to render this map's world. The editor uses it to convert
    // mouse input to world space; maps without a camera keep identity.
    virtual Camera2D GetViewCamera() const { return Camera2D{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f }; }

    // 3D maps expose their gameplay camera so the editor can project selection
    // gizmos and unproject mouse rays.
    virtual bool b_Uses3DView() const { return false; }
    virtual Camera3D GetViewCamera3D() const
    {
        return Camera3D{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f, CAMERA_PERSPECTIVE };
    }

    // Editor render override. The host sets this while the project is stopped;
    // maps render through GetRenderCamera3D()/BeginView3D(), so viewport
    // navigation never leaks into gameplay code.
    void SetEditorViewCamera3D(const Camera3D& camera);
    void ClearEditorViewCamera3D();
    bool b_HasEditorViewCamera3D() const { return m_bHasEditorViewCamera3D; }
    Camera3D GetRenderCamera3D() const;
    void BeginView3D() const;
    static void EndView3D();

    // Optional mutable camera for editor viewport navigation. Maps that return
    // a pointer must render their world through it.
    virtual Camera2D* GetMutableViewCamera() { return nullptr; }

    void SetTargetFPS(int fps);
    int GetTargetFPS() const;

    // Editor entity registry. Register after the entity is fully constructed
    // and its storage is stable. Clear before destroying or moving entities.
    t_EditorEntity* RegisterEditorEntity(
        std::string_view name, Vector2* position, Vector2* size = nullptr, uint64_t id = 0);
    t_EditorEntity* RegisterEditorEntity(t_EditorEntity entity);

    // One-line registration. `size` stays a pointer so Inspector resizing
    // writes through to the value the game draws with.
    t_EditorEntity* AddEntity(
        std::string_view name, Vector2* position, Vector2* size = nullptr, const t_EntityOptions& options = {});
    t_EditorEntity* AddEntity(
        std::string_view name, Vector3* position, Vector3* size = nullptr, const t_EntityOptions& options = {});
    bool UnregisterEditorEntity(uint64_t id);
    void ClearEditorEntities();
    std::span<const t_EditorEntity> GetEditorEntities() const;
    std::span<t_EditorEntity> GetEditorEntities();

    // True when the map runs inside the standalone runtime (game.exe), false
    // inside the editor. Templates use this to capture the cursor only in a
    // real game, never while editing.
    virtual void SetStandaloneMode(bool b_Standalone);
    bool b_IsStandalone() const;

    // Hook for MapManager: injects a function that executes a map transition.
    // Maps call RequestGotoMap to trigger transitions safely (no global/static).
    void SetTransitionCallback(std::function<void(std::string_view, bool)> callback);

    void SetExitCallback(std::function<void()> callback);

protected:
    // Helper maps can call to request a transition (executes callback if provided)
    void RequestGotoMap(std::string_view map_id, bool force_reload = false) const;

    // Helper maps can call to request shutdown (executes callback if provided)
    void RequestExit();
};
