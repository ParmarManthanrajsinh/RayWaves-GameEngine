#include "ThirdPersonMap.h"
#include "rw.h"

// Module entry point. Add a level with one AddLevel<T>() line; RW_GAME
// generates the three exports the editor and runtime load.
class ThirdPersonGame : public rw::Game
{
public:
    ThirdPersonGame()
    {
        AddLevel<ThirdPersonMap>("ThirdPersonMap", "Third-person sandbox");
        Start("ThirdPersonMap");
    }
};

RW_GAME(ThirdPersonGame)
