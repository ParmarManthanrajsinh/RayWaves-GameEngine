#pragma once
#include "../Engine/GameMap.h"
#include "FirstPersonCamera.h"
#include "SceneTarget.h"
#include <raylib.h>

class FirstPersonMap : public GameMap
{
public:
    FirstPersonMap();
    ~FirstPersonMap() override;

    void SetStandaloneMode(bool b_Standalone) override;
    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;

    void SaveState(StateBag& out) const override;
    void LoadState(const StateBag& in) override;

private:
    void DrawHud(int width, int height) const;
    void DrawMarkers() const;
    Vector3 GetMoveInput() const;

    FirstPersonCamera m_Camera;
    SceneTarget m_SceneTarget;

    static constexpr float ARENA_HALF_SIZE = 28.0f;
    static constexpr float EYE_HEIGHT = 1.7f;
    static constexpr float WALK_SPEED = 6.0f;
    static constexpr float SPRINT_MULT = 1.7f;
};
