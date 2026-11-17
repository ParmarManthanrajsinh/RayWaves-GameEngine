#pragma once
#include "Engine/GameMap.h"
#include "GameCamera.h"
#include "Player.h"
#include "Slime.h"
#include <raylib.h>
#include <vector>

struct GroundTile
{
    Rectangle Rect;
    int32_t Type;
};

class DemoLevel : public GameMap
{
private:
    static Rectangle GetTileRect(int32_t Col, int32_t Row);

    void DrawBackground();
    void DrawTrees(float InFloorY);
    void DrawGround(float InFloorY);
    static void DrawSparkles();
    void DrawSlimes();
    void UnloadAssets();

    Player m_Player;
    GameCamera m_Camera;
    std::vector<Slime> m_Slimes;

    Vector2 m_PlayerEditorSize{ 64.0f, 64.0f };
    Vector2 m_SlimeEditorSize{ 72.0f, 72.0f };

    Texture2D m_TilesetTex;
    Texture2D m_SlimeTexture;
    Sound m_SlimeDeathSound;
    std::vector<Texture2D> m_BackgroundLayers;
    std::vector<GroundTile> m_GroundTiles;

    static constexpr float GRAVITY = 1200.0f;

public:
    DemoLevel();
    ~DemoLevel() override;
    void Initialize() override;
    void Update(float DeltaTime) override;
    void Draw() override;
    Camera2D GetViewCamera() const override;
    Camera2D* GetMutableViewCamera() override;

    void SaveState(StateBag& out) const override;
    void LoadState(const StateBag& in) override;

    void Reset();
};
