#pragma once
#include "../Engine/GameMap.h"
#include <raylib.h>
#include <string>

class DemoMainMenu : public GameMap
{
private:
    Font m_TitleFont;
    Sound m_SelectSound;
    
    // UI State
    int m_SelectedOption = 0;
    const int OPTION_PLAY = 0;
    const int OPTION_EXIT = 1;
    
    // Animation
    float m_Time = 0.0f;
    float m_PulseScale = 1.0f;

public:
    DemoMainMenu();
    ~DemoMainMenu() override = default;

    void Initialize() override;
    void Update(float delta_time) override;
    void Draw() override;
};
