#include "DemoLevel.h"
#include "../Engine/AssetResolver.h"
#include <iostream>
#include <algorithm>
#include <cmath>

constexpr float TileSrcSize = 16.0f;
constexpr float TileRenderSize = 32.0f;
constexpr float FloorY = 405.0f;

DemoLevel::DemoLevel()
    : GameMap("Platformer Demo"),
      m_TilesetTex{ LoadTexture("") }, 
      m_SlimeTexture{ LoadTexture("") }, 
      m_SlimeDeathSound{ LoadSound("") } 
{
}

DemoLevel::~DemoLevel()
{
    UnloadTexture(m_TilesetTex);
    UnloadTexture(m_SlimeTexture);
    UnloadSound(m_SlimeDeathSound);
    
    for (auto& Tex : m_BackgroundLayers)
    {
        UnloadTexture(Tex);
    }
    m_BackgroundLayers.clear();
}

void DemoLevel::Initialize()
{
    m_Player.Initialize(AssetResolver::Resolve("player.png").c_str());
    m_TilesetTex = LoadTexture(AssetResolver::Resolve("tileset.png").c_str());
    m_SlimeTexture = LoadTexture(AssetResolver::Resolve("slime.png").c_str());
    m_SlimeDeathSound = LoadSound(AssetResolver::Resolve("Sounds/slime_death.wav").c_str());

    m_BackgroundLayers.clear();
    m_BackgroundLayers.emplace_back(LoadTexture(AssetResolver::Resolve("background_0.png").c_str()));
    m_BackgroundLayers.emplace_back(LoadTexture(AssetResolver::Resolve("background_1.png").c_str()));
    m_BackgroundLayers.emplace_back(LoadTexture(AssetResolver::Resolve("background_2.png").c_str()));

    Reset();
    std::cout << "[DemoLevel] Assets Loaded & Initialized" << std::endl;
}

void DemoLevel::Reset()
{
    m_Player.Reset({ 100, 300 });
    
    m_Camera.Initialize(m_Player.GetPosition(), 2.5f);
    m_Camera.SetMinZoom(2.5f);
    
    float LevelLeft = -10.0f * TileRenderSize;
    float LevelRight = 60.0f * TileRenderSize;
    float LevelTop = 0.0f;
    float LevelBottom = 350.0f;
    m_Camera.SetBounds(LevelLeft, LevelRight, LevelTop, LevelBottom);

    m_GroundTiles.clear();
    for (int32_t i = -10; i < 60; ++i)
    {
        m_GroundTiles.push_back
        ({
            { 
                static_cast<float>(i) * TileRenderSize,
                FloorY,
                TileRenderSize,
                TileRenderSize 
            }, 0
        });
    }
    
    // Initialize slimes - Y is center of slime, so offset by half render size (36) from ground
    m_Slimes.clear();
    float SlimeGroundY = FloorY + 15.f;
    
    Slime Slime1;
    Slime1.Initialize(m_SlimeTexture, m_SlimeDeathSound, { 400, SlimeGroundY });
    Slime1.SetPatrolBounds(300, 500);
    m_Slimes.push_back(Slime1);
    
    Slime Slime2;
    Slime2.Initialize(m_SlimeTexture, m_SlimeDeathSound, { 800, SlimeGroundY });
    Slime2.SetPatrolBounds(700, 900);
    m_Slimes.push_back(Slime2);
    
    Slime Slime3;
    Slime3.Initialize(m_SlimeTexture, m_SlimeDeathSound, { 1200, SlimeGroundY });
    Slime3.SetPatrolBounds(1100, 1400);
    m_Slimes.push_back(Slime3);
}

inline Rectangle DemoLevel::GetTileRect(int32_t Col, int32_t Row) const
{
    return 
    { 
        Col * TileSrcSize, 
        Row * TileSrcSize, 
        TileSrcSize, 
        TileSrcSize 
    };
}

inline int32_t DemoLevel::PseudoRandom(int32_t X, int32_t Seed) const
{
    int32_t Hash = X * 374761393 + Seed * 668265263;
    Hash = (Hash ^ (Hash >> 13)) * 1274126177;
    return Hash ^ (Hash >> 16);
}

void DemoLevel::SaveState(StateBag& out) const
{
    m_Player.SaveState(out);
}

void DemoLevel::LoadState(const StateBag& in)
{
    m_Player.LoadState(in);
}

void DemoLevel::Update(float DeltaTime)
{
    m_Camera.UpdateViewport(m_SceneWidth, m_SceneHeight);

    m_Player.HandleInput(DeltaTime);
    m_Player.Update(DeltaTime);
    m_Player.ApplyGravity(DeltaTime, GRAVITY);
    m_Player.ResolveCollisions(DeltaTime, m_GroundTiles);
    
    float LevelLeft = -10.0f * TileRenderSize + 32.0f;
    float LevelRight = 60.0f * TileRenderSize - 32.0f;
    m_Player.ClampToLevel(LevelLeft, LevelRight);
    
    m_Camera.FollowTarget(m_Player.GetPosition(), DeltaTime, 5.0f);
    
    // Update slimes
    for (auto& SlimeEnemy : m_Slimes)
    {
        SlimeEnemy.Update(DeltaTime);
    }
    
    // Check player attack vs slimes
    if (m_Player.IsAttacking())
    {
        Rectangle AttackHitbox = m_Player.GetAttackHitbox();
        for (auto& SlimeEnemy : m_Slimes)
        {
            if (SlimeEnemy.IsAlive() && !SlimeEnemy.IsDying())
            {
                Rectangle SlimeHitbox = SlimeEnemy.GetHitbox();
                if (CheckCollisionRecs(AttackHitbox, SlimeHitbox))
                {
                    SlimeEnemy.TakeDamage();
                }
            }
        }
    }
    
    if (m_Player.GetPosition().y > 1000)
    {
        Reset();
    }
}

void DemoLevel::Draw()
{
    ClearBackground(Color{ 20, 24, 46, 255 });
    m_Camera.Begin();

    DrawBackground();
    DrawTrees(FloorY);
    DrawGround(FloorY);
    DrawSparkles();
    DrawSlimes();
    m_Player.Draw();
    m_Camera.End();
}

void DemoLevel::DrawBackground()
{
    DrawRectangle(-10000, -10000, 20000, 20000, Color{ 40, 48, 70, 255 });

    float GroundCamY = 300.0f;
    Vector2 CamTarget = m_Camera.GetTarget();

    for (size_t i = 0; i < m_BackgroundLayers.size(); ++i)
    {
        Texture2D& Tex = m_BackgroundLayers[i];
        
        if (Tex.id <= 0)
        {
            continue;
        }
        
        float Speed = 0.05f + (i * 0.15f);
        float Scale = 2.0f;
        float ScaledW = static_cast<float>(Tex.width) * Scale;
        float ScaledH = static_cast<float>(Tex.height) * Scale;
        
        float BgX = CamTarget.x * (1.0f - Speed);
        float AlignedX = floor(BgX / ScaledW) * ScaledW;
        
        float Overlap = 64.0f;
        float Offset = (FloorY - ScaledH) - (GroundCamY * (1.0f - Speed)) + Overlap;
        float BgY = CamTarget.y * (1.0f - Speed) + Offset;
        
        for (int32_t k = -1; k <= 2; ++k)
        {
            DrawTexturePro
            (
                Tex,
                { 0, 0, static_cast<float>(Tex.width), static_cast<float>(Tex.height) },
                { AlignedX + k * ScaledW, BgY, ScaledW, ScaledH },
                { 0, 0 },
                0,
                WHITE
            );
        }
    }
    
    DrawRectangle
    (
        -2000, 
        static_cast<int>(FloorY + TileRenderSize), 
        5000, 
        1000, 
        Color{ 15, 12, 22, 255 }
    );
}

void DemoLevel::DrawTrees(float InFloorY)
{
    auto DrawTree = [&](float X, float Y)
    {
        Rectangle Src = { 160, 0, 128, 128 };
        Rectangle Dst = { X - 128, Y - 256 + 32, 256, 256 };
        DrawTexturePro(m_TilesetTex, Src, Dst, { 0, 0 }, 0, WHITE);
    };
    
    DrawTree(200, InFloorY);
    DrawTree(900, InFloorY);
    DrawTree(1500, InFloorY);
}

void DemoLevel::DrawGround(float InFloorY)
{
    const int32_t SurfacePattern[] = { 9, 10, 9, 4, 5, 6, 7, 8 };
    const int32_t SurfacePatternLen = 8;
    const int32_t UnderPattern[] = { 8, 9 };
    const int32_t UnderPatternLen = 2;
    const int32_t DeepUnderPattern[] = { 0, 1, 2 };
    const int32_t TileOffset[] = { 4, 5, 3, 6, 3, 5 };
    int32_t DeepUnderPatternLen = 3;

    int32_t TileIndex = 0;
    
    for (const auto& Tile : m_GroundTiles)
    {
        int32_t SurfaceCol = SurfacePattern[TileIndex % SurfacePatternLen];
        DrawTexturePro
        (
            m_TilesetTex, 
            GetTileRect(SurfaceCol, 8), 
            Tile.Rect, 
            { 0, 0 }, 
            0, 
            WHITE
        );
        
        for (int32_t Depth = 1; Depth <= 6; ++Depth)
        {
            Rectangle DeepRect = Tile.Rect;
            DeepRect.y += Depth * TileRenderSize;

            if (Depth <= 1)
            {
                int32_t UnderCol = UnderPattern[TileIndex % UnderPatternLen];
                DrawTexturePro
                (
                    m_TilesetTex, 
                    GetTileRect(UnderCol, 9), 
                    DeepRect, 
                    { 0, 0 }, 
                    0, 
                    WHITE
                );
            }
            else
            {
                int32_t UnderCol = DeepUnderPattern[TileIndex % 3];
                DrawTexturePro
                (
                    m_TilesetTex, 
                    GetTileRect(UnderCol, 9), 
                    DeepRect, 
                    { 0, 0 }, 
                    0, 
                    WHITE
                );
            }
        }
        
        TileIndex++;
    }
}

void DemoLevel::DrawSparkles()
{
    double Time = GetTime();
    
    for (int32_t s = 0; s < 12; ++s)
    {
        float SparkleX = 
            50.0f + s * 120.0f + static_cast<float>(sin(Time * 0.5 + s)) * 8.0f;

        float SparkleY = 
            280.0f + static_cast<float>(cos(Time * 0.3 + s * 0.7)) * 40.0f;

        float Alpha = (static_cast<float>(sin(Time * 2.0 + s)) + 1.0f) * 0.4f;
        
        DrawCircle
        (
            static_cast<int>(SparkleX),
            static_cast<int>(SparkleY),
            2,
            Color{ 255, 230, 180, static_cast<unsigned char>(Alpha * 200) }
        );
    }
}

void DemoLevel::DrawSlimes()
{
    for (auto& SlimeEnemy : m_Slimes)
    {
        SlimeEnemy.Draw();
    }
}

void DemoLevel::DrawDebugTileset()
{
    float Scale = 2.0f;
    float StartX = 50;
    float StartY = 80;
    
    DrawTextureEx(m_TilesetTex, { StartX, StartY }, 0, Scale, WHITE);
    DrawRectangleLinesEx
    (
        Rectangle
        { 
            StartX, StartY, 
            m_TilesetTex.width * Scale, 
            m_TilesetTex.height * Scale 
        },
        2.0f,
        YELLOW
    );
    
    float YLedge = 64.0f;
    float YGround = 128.0f;
    
    DrawRectangleLinesEx
    (
        Rectangle
        { 
            StartX, StartY + (YGround * Scale), 
            m_TilesetTex.width * Scale, 
            16 * Scale 
        },
        2.0f,
        Color{ 255, 0, 0, 255 }
    );
    DrawRectangleLinesEx
    (
        Rectangle
        {
            StartX, StartY + (YLedge * Scale), 
            m_TilesetTex.width * Scale, 
            16 * Scale 
        },
        2.0f,
        Color{ 0, 255, 0, 255 }
    );
}
