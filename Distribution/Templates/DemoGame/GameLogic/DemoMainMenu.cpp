#include "DemoMainMenu.h"
#include "Engine/AssetResolver.h"
#include "Engine/MapManager.h"
#include "Engine/raygui.h"
#include <iostream>
#include <cmath>

DemoMainMenu::DemoMainMenu() 
    : GameMap("Main Menu")
{
}

void DemoMainMenu::Initialize()
{
    m_TitleFont = GetFontDefault();
    m_SelectSound = LoadSound(AssetResolver::Resolve("Sounds/menu_select.wav").c_str());
    
    std::cout << "[DemoMainMenu] Initialized" << '\n';
}

void DemoMainMenu::Update(float DeltaTime)
{
    m_Time += DeltaTime;
    m_PulseScale = 1.0f + (sin(m_Time * 3.0f) * 0.05f);

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
            std::cout << "Exit Requested" << '\n';
            CloseWindow();
        }
    }
}

void DemoMainMenu::Draw()
{
    float ScreenWidth = m_SceneWidth;
    float ScreenHeight = m_SceneHeight;

	// Draw Background
    ClearBackground(BLACK);

    GuiSetFont(m_TitleFont);
    int oldTextSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    
    // Draw Title
    const char* Title = "Shadow Woods";
    Vector2 TitleSize = MeasureTextEx(m_TitleFont, Title, 80, 2);
    
    GuiSetStyle(DEFAULT, TEXT_SIZE, 80);
    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(Color{ 255, 200, 100, 255 }));
    GuiLabel(Rectangle{ (ScreenWidth - TitleSize.x) / 2.0f, 250.0f, TitleSize.x, 80.0f }, Title);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 40);
    
    // Draw Menu Options using raygui
    float StartY = 400.0f;
    float Padding = 60.0f;
    const float btnWidth = 250.0f;
    const float btnHeight = 50.0f;

    // Set Button Styles
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(Color{ 30, 30, 30, 255 }));
    GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(GRAY));
    GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL, ColorToInt(Color{ 50, 50, 50, 255 }));
    
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(Color{ 50, 50, 50, 255 }));
    GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(WHITE));
    GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED, ColorToInt(ORANGE));
    
    GuiSetStyle(BUTTON, BASE_COLOR_PRESSED, ColorToInt(Color{ 20, 20, 20, 255 }));
    GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED, ColorToInt(ORANGE));
    GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED, ColorToInt(ORANGE));

    // PLAY GAME
    if (m_SelectedOption == 0) GuiSetState(STATE_FOCUSED);
    if (GuiButton(Rectangle{ (ScreenWidth - btnWidth) / 2.0f, StartY, btnWidth, btnHeight }, "PLAY GAME") != 0)
    {
        PlaySound(m_SelectSound);
        RequestGotoMap("DemoLevel");
    }
    GuiSetState(STATE_NORMAL);

    // EXIT
    if (m_SelectedOption == 1) GuiSetState(STATE_FOCUSED);
    if (GuiButton(Rectangle{ (ScreenWidth - btnWidth) / 2.0f, StartY + Padding, btnWidth, btnHeight }, "EXIT") != 0)
    {
        std::cout << "Exit Requested" << '\n';
        CloseWindow();
    }
    GuiSetState(STATE_NORMAL);
    
    // Restore default style
    GuiSetStyle(DEFAULT, TEXT_SIZE, oldTextSize);
}
