#pragma once
#include <raylib.h>
#include <vector>
#include <cstdint>
#include "../Engine/GameState.h"

struct GroundTile;

class Player
{
public:
    Player();
    ~Player();
    
    void Initialize(const char* TexturePath);
    void LoadSounds();
    void Reset(Vector2 StartPosition);
    void HandleInput(float DeltaTime);
    void Update(float DeltaTime);
    void ApplyGravity(float DeltaTime, float Gravity);
    void ResolveCollisions(float DeltaTime, const std::vector<GroundTile>& Tiles);
    void ClampToLevel(float LevelLeft, float LevelRight);
    void Draw();
    
    void SaveState(StateBag& out) const;
    void LoadState(const StateBag& in);
    
    Vector2 GetPosition() const { return m_Position; }
    Vector2 GetVelocity() const { return m_Velocity; }
    bool IsGrounded() const { return m_bIsGrounded; }
    bool IsAttacking() const { return m_bIsAttacking; }
    bool IsFacingRight() const { return m_bFacingRight; }
    Rectangle GetAttackHitbox() const;
    
    void SetPosition(Vector2 NewPosition) { m_Position = NewPosition; }
    void SetVelocity(Vector2 NewVelocity) { m_Velocity = NewVelocity; }

private:
    Texture2D m_Texture;
    Vector2 m_Position;
    Vector2 m_Velocity;
    bool m_bIsGrounded;
    bool m_bFacingRight;
    
    bool m_bIsAttacking;
    float m_AttackTimer;
    int32_t m_AttackFrame;
    
    // Sounds
    Sound m_JumpSound;
    Sound m_AttackSound;
    
    static constexpr float SPEED = 200.0f;
    static constexpr float JUMP_FORCE = -550.0f;
    static constexpr float HITBOX_WIDTH = 64.0f;
    static constexpr float HITBOX_HEIGHT = 64.0f;
    static constexpr float HITBOX_OFFSET_X = 16.0f;
    static constexpr float HITBOX_OFFSET_Y = 16.0f;
    static constexpr float ATTACK_DURATION = 0.8f;
    static constexpr int32_t ATTACK_FRAMES = 8;
};
