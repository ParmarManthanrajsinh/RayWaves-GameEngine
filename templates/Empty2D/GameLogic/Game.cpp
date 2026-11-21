#include "EmptyLevel.h"
#include "rw.h"

// Module entry point. Add a level with one AddLevel<T>() line; RW_GAME
// generates the three exports the editor and runtime load.
class EmptyGame : public rw::Game
{
public:
    EmptyGame()
    {
        AddLevel<EmptyLevel>("EmptyLevel", "Blank starter level");
        Start("EmptyLevel");
    }
};

RW_GAME(EmptyGame)
