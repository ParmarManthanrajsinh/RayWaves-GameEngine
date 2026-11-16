#pragma once
#include "../Engine/GameMap.h"
#include "Player.h"
#include "SceneTarget.h"
#include "ThirdPersonCamera.h"
#include <raylib.h>
#include <vector>

class ThirdPersonMap : public GameMap
{
public:
    ThirdPersonMap();
    ~ThirdPersonMap() override;

    void SetStandaloneMode(bool b_Standalone) override;
    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;

    void SaveState(StateBag& out) const override;
    void LoadState(const StateBag& in) override;

private:
    void BuildArena();
    void AddBox(Vector3 center, Vector3 size);
    void DrawArena() const;
    void DrawHud(int width, int height) const;
    Vector3 GetMoveInput() const;

    Player m_Player;
    ThirdPersonCamera m_Camera;
    SceneTarget m_SceneTarget;
    std::vector<BoundingBox> m_Colliders;

    static constexpr float ARENA_HALF_SIZE = 20.0f;
};
