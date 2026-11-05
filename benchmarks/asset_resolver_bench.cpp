#include "support/bench.hpp"

#include "AssetResolver.h"

BENCHMARK_CASE("AssetResolver::Resolve", 1000)
{
    static bool b_Initialized = false;
    if (!b_Initialized)
    {
        AssetResolver::SetProjectAssetPath("C:/MyGame/Assets");
        b_Initialized = true;
    }

    for (int i = 0; i < 1000; ++i)
    {
        volatile auto result = AssetResolver::Resolve("textures/player.png");
        (void)result;
    }
}
