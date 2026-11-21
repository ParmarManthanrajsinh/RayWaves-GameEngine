#include "DemoLevel.h"
#include "DemoMainMenu.h"
#include "rw.h"

// Module entry point. Add a level with one AddLevel<T>() line; RW_GAME
// generates the three exports the editor and runtime load.
//
// Asset paths are resolved through AssetResolver, so loading with
// LoadTexture("...") works from any working directory.
class SlimeQuestGame : public rw::Game
{
public:
    SlimeQuestGame()
    {
        AddLevel<DemoMainMenu>("DemoMainMenu", "Title screen");
        AddLevel<DemoLevel>("DemoLevel", "Slime level");
        Start("DemoMainMenu");
    }
};

RW_GAME(SlimeQuestGame)
