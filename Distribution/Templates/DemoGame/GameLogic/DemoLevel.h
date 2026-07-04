#pragma once
#include "Engine/GameMap.h"
#include "Player.h"
#include "GameCamera.h"
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
    Rectangle GetTileRect(int32_t Col, int32_t Row) const;
    int32_t PseudoRandom(int32_t X, int32_t Seed) const;

    void DrawBackground();
    void DrawTrees(float InFloorY);
    void DrawGround(float InFloorY);
    void DrawSparkles();
    void DrawSlimes();
    void DrawDebugTileset();

    Player m_Player;
    GameCamera m_Camera;
    std::vector<Slime> m_Slimes;
    
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
    
    void SaveState(StateBag& out) const override;
    void LoadState(const StateBag& in) override;
    
    void Reset();
};
