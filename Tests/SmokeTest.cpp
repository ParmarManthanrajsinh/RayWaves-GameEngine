#include <iostream>
#include <cstdlib>
#include <memory>
#include "../Game/DllLoader.h"
#include "../Engine/GameMap.h"
#include "../Engine/GameState.h"

typedef GameMap* (*CreateGameMapFunc)();
typedef void (*DestroyGameMapFunc)(GameMap*);

int main(int argc, char** argv) {
    std::cout << "Starting Smoke Test: 50 Hot Reloads" << std::endl;
    
    for (int i = 0; i < 50; ++i) {
        std::cout << "\n--- Iteration " << i + 1 << "/50 ---" << std::endl;
        
        // 1. Rebuild GameLogic (simulating a code change)
        int buildResult = std::system("cmake --build . --target GameLogic");
        if (buildResult != 0) {
            std::cerr << "Build failed. Are you running this from the CMake build directory?" << std::endl;
            return 1;
        }

        // 2. Load the DLL
        DllHandle dll = LoadDll("GameLogic.dll");
        if (!dll.handle) {
            std::cerr << "Failed to load GameLogic.dll" << std::endl;
            return 1;
        }
        std::cout << "Loaded shadow DLL: " << dll.shadow_path << std::endl;

        // 3. Resolve symbols
        CreateGameMapFunc createMap = (CreateGameMapFunc)GetDllSymbol(dll, "CreateGameMap");
        DestroyGameMapFunc destroyMap = (DestroyGameMapFunc)GetDllSymbol(dll, "DestroyGameMap");
        if (!createMap || !destroyMap) {
            std::cerr << "Failed to find CreateGameMap/DestroyGameMap symbols" << std::endl;
            UnloadDll(dll);
            return 1;
        }

        // 4. Create map via DLL factory
        GameMap* map = createMap();
        if (map) {
            // Exercise the vtable
            map->Initialize();
            
            // Exercise SaveState/LoadState to catch CRT boundaries across DLL
            StateBag bag;
            map->SaveState(bag);
            map->LoadState(bag);

            // 5. Destroy map via DLL destroyer BEFORE unload
            // IMPORTANT: Must use DestroyGameMap (not delete) to avoid cross-CRT heap corruption.
            // The object was new'd in the DLL's CRT, so it must be delete'd there too.
            destroyMap(map);
        }

        // 6. Unload DLL (triggers shadow copy deletion)
        UnloadDll(dll);
        std::cout << "Unloaded successfully." << std::endl;
    }
    
    std::cout << "\nSmoke Test Passed: 50 iterations complete with no crash." << std::endl;
    return 0;
}
