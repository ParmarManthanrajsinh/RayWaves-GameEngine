#include "EmptyMap.h"
#include <iostream>
#include <cmath>

void EmptyMap::Initialize() 
{
    std::cout << "EmptyMap initialized\n";
    playerPos = { 400.0f, 300.0f };
    time = 0.0f;
}

void EmptyMap::Update(float delta_time) 
{
    time += delta_time;
    float speed = 200.0f * delta_time;

    if (IsKeyDown(KEY_RIGHT)) playerPos.x += speed;
    if (IsKeyDown(KEY_LEFT)) playerPos.x -= speed;
    if (IsKeyDown(KEY_DOWN)) playerPos.y += speed;
    if (IsKeyDown(KEY_UP)) playerPos.y -= speed;
}

void EmptyMap::Draw() 
{
    // Dark background
    Color bg = { 24, 24, 24, 255 }; // #181818
    ClearBackground(bg);

    // Draw grid
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Color gridColor = { 255, 255, 255, 10 }; // Faint white
    for (int i = 0; i < screenWidth; i += 40)
    {
        DrawLine(i, 0, i, screenHeight, gridColor);
    }
    for (int i = 0; i < screenHeight; i += 40)
    {
        DrawLine(0, i, screenWidth, i, gridColor);
    }

    // Draw animated/pulsating welcome text
    float textYOffset = sinf(time * 3.0f) * 10.0f;
    const char* title = "Welcome to RayWaves Engine";
    int titleWidth = MeasureText(title, 30);
    DrawText(title, (screenWidth / 2) - (titleWidth / 2), 50 + static_cast<int>(textYOffset), 30, LIGHTGRAY);
    
    const char* subtext = "Edit GameLogic to start";
    int subWidth = MeasureText(subtext, 20);
    DrawText(subtext, (screenWidth / 2) - (subWidth / 2), 90 + static_cast<int>(textYOffset), 20, GRAY);

    // Draw interactive player box
    Rectangle playerRec = { playerPos.x, playerPos.y, 40, 40 };
    DrawRectangleRec(playerRec, { 50, 150, 255, 255 }); // Nice blue
    DrawRectangleLinesEx(playerRec, 2, WHITE);
}
