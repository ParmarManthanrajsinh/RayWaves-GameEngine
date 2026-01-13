#include "DemoMainMenu.h"
#include "../Engine/MapManager.h"
#include <iostream>
#include <cmath>

DemoMainMenu::DemoMainMenu() 
    : GameMap("Main Menu")
{
}

void DemoMainMenu::Initialize()
{
    m_TitleFont = LoadFontEx("Assets/EngineContent/Roboto-Regular.ttf", 64, 0, 0);
    m_SelectSound = LoadSound("Assets/Sounds/menu_select.wav");
    
    std::cout << "[DemoMainMenu] Initialized" << std::endl;
}

void DemoMainMenu::Update(float DeltaTime)
{
    m_Time += DeltaTime;
    m_PulseScale = 1.0f + sin(m_Time * 3.0f) * 0.05f;

    // Navigation
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    {
        m_SelectedOption = (m_SelectedOption + 1) % 2;
    }
    
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    {
        m_SelectedOption = (m_SelectedOption - 1 + 2) % 2;
    }

    // Selection
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        PlaySound(m_SelectSound);
        if (m_SelectedOption == OPTION_PLAY)
        {
            RequestGotoMap("DemoLevel");
        }
        else if (m_SelectedOption == OPTION_EXIT)
        {
            std::cout << "Exit Requested" << std::endl;
            CloseWindow();
        }
    }
}

void DemoMainMenu::Draw()
{
    // Draw Background with proper scaling (cover mode - fills screen while maintaining aspect ratio)
    float ScreenWidth = static_cast<float>(GetScreenWidth());
    float ScreenHeight = static_cast<float>(GetScreenHeight());

    // Draw Title
    const char* Title = "Shadow Woods";
    Vector2 TitleSize = MeasureTextEx(m_TitleFont, Title, 80, 2);
    Vector2 TitlePos = 
    {
        (ScreenWidth - TitleSize.x) / 2.0f,
        250.0f
    };
    
	// Draw Background
    ClearBackground(BLACK);

    // Draw Shadow
    DrawTextEx(m_TitleFont, Title, Vector2{ TitlePos.x + 4, TitlePos.y + 4 }, 80, 2, Color{ 0, 0, 0, 180 });
    // Draw Text
    DrawTextEx(m_TitleFont, Title, TitlePos, 80, 2, Color{ 255, 200, 100, 255 });

    // Draw Menu Options
    const char* Options[] = { "PLAY GAME", "EXIT" };
    float StartY = 400.0f;
    float Padding = 60.0f;

    for (int32_t i = 0; i < 2; ++i)
    {
        bool bIsSelected = (i == m_SelectedOption);
        Color TextColor = bIsSelected ? WHITE : GRAY;
        float FontSize = bIsSelected ? 40.0f * m_PulseScale : 40.0f;
        
        Vector2 TextSize = MeasureTextEx(m_TitleFont, Options[i], FontSize, 2);
        Vector2 TextPos = 
        {
            (GetScreenWidth() - TextSize.x) / 2.0f,
            StartY + (i * Padding)
        };

        if (bIsSelected)
        {
            DrawTextEx(m_TitleFont, ">", Vector2{ TextPos.x - 30, TextPos.y }, FontSize, 2, ORANGE);
        }
        
        DrawTextEx(m_TitleFont, Options[i], TextPos, FontSize, 2, TextColor);
    }
}
