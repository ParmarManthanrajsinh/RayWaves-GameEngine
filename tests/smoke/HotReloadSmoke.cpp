#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Runtime/DllLoader.h"
#include "Engine/GameMap.h"
#include "Engine/GameState.h"

namespace
{
    int ParseIterations(int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--iterations" && i + 1 < argc)
            {
                return std::max(1, std::atoi(argv[++i]));
            }
        }
        return 3;
    }
}

int main(int argc, char** argv)
{
    const int iterations = ParseIterations(argc, argv);
    std::cout << "Hot-reload smoke test: " << iterations << " load/unload cycles\n";

    // Build once, reload many times. Rebuilding inside the loop made the old
    // smoke test slow and dependent on the working directory.
    if (std::system("cmake --build . --target GameLogic") != 0)
    {
        std::cerr << "GameLogic build failed. Run this from the CMake build directory.\n";
        return 1;
    }

    for (int i = 0; i < iterations; ++i)
    {
        DllHandle dll = LoadDll("GameLogic.dll");
        if (dll.handle == nullptr)
        {
            std::cerr << "Failed to load GameLogic.dll\n";
            return 1;
        }

        auto create_map = reinterpret_cast<GameMap* (*)()>(GetDllSymbol(dll, "CreateGameMap"));
        auto destroy_map = reinterpret_cast<void (*)(GameMap*)>(GetDllSymbol(dll, "DestroyGameMap"));
        auto abi_version = reinterpret_cast<uint32_t (*)()>(GetDllSymbol(dll, "GetGameLogicAbiVersion"));

        if (create_map == nullptr || destroy_map == nullptr || abi_version == nullptr)
        {
            std::cerr << "Missing GameLogic ABI exports\n";
            UnloadDll(dll);
            return 1;
        }

        if (abi_version() != RAYWAVES_GAMELOGIC_ABI_VERSION)
        {
            std::cerr << "GameLogic ABI mismatch: " << abi_version()
                      << " (DLL) vs " << RAYWAVES_GAMELOGIC_ABI_VERSION << " (engine)\n";
            UnloadDll(dll);
            return 1;
        }

        GameMap* map = create_map();
        if (map != nullptr)
        {
            map->Initialize();

            StateBag bag;
            map->SaveState(bag);
            map->LoadState(bag);

            // Objects created in the DLL CRT must be destroyed there too.
            destroy_map(map);
        }

        UnloadDll(dll);
        std::cout << "Cycle " << (i + 1) << "/" << iterations << " passed\n";
    }

    std::cout << "Hot-reload smoke test passed\n";
    return 0;
}
