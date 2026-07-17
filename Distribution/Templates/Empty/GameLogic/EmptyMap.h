#pragma once
#include "../Engine/MapManager.h"
#include <raylib.h>

class EmptyMap : public GameMap
{
private:
    Vector2 playerPos;
    float time;

public:
    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;
};
