#include <iostream>
#include <cstdlib>
#include <memory>
#include "../Game/DllLoader.h"
#include "../Engine/GameMap.h"
#include "../Engine/GameState.h"

typedef GameMap* (*CreateGameMapFunc)();

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

        // 3. Resolve symbol and create map
        CreateGameMapFunc createMap = (CreateGameMapFunc)GetDllSymbol(dll, "CreateGameMap");
        if (!createMap) {
            std::cerr << "Failed to find CreateGameMap symbol" << std::endl;
            UnloadDll(dll);
            return 1;
        }

        std::unique_ptr<GameMap> map(createMap());
        if (map) {
            // Exercise the vtable
            map->Initialize();
            
            // Exercise SaveState/LoadState to catch CRT boundaries across DLL
            StateBag bag;
            map->SaveState(bag);
            map->LoadState(bag);
        }

        // 4. Destroy map BEFORE unload
        map.reset();

        // 5. Unload DLL (triggers shadow copy deletion)
        UnloadDll(dll);
        std::cout << "Unloaded successfully." << std::endl;
    }
    
    std::cout << "\nSmoke Test Passed: 50 iterations complete with no crash." << std::endl;
    return 0;
}
