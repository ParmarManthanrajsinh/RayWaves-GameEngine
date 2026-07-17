#include <iostream>
#include <cstdlib>
#include <memory>
#include "../Game/DllLoader.h"
#include "../Engine/GameMap.h"
#include "../Engine/GameState.h"

typedef GameMap* (*CreateGameMapFunc)();
typedef void (*DestroyGameMapFunc)(GameMap*);

int main(int argc, char** argv) {
    std::cout << "Starting Smoke Test: 50 Hot Reloads" << '\n';
    
    for (int i = 0; i < 50; ++i) {
        std::cout << "\n--- Iteration " << i + 1 << "/50 ---" << '\n';
        
        // 1. Rebuild GameLogic (simulating a code change)
        int buildResult = std::system("cmake --build . --target GameLogic");
        if (buildResult != 0) {
            std::cerr << "Build failed. Are you running this from the CMake build directory?" << '\n';
            return 1;
        }

        // 2. Load the DLL
        DllHandle dll = LoadDll("GameLogic.dll");
        if (dll.handle == nullptr) {
            std::cerr << "Failed to load GameLogic.dll" << '\n';
            return 1;
        }
        std::cout << "Loaded shadow DLL: " << dll.shadow_path << '\n';

        // 3. Resolve symbols
        auto createMap = reinterpret_cast<CreateGameMapFunc>(GetDllSymbol(dll, "CreateGameMap"));
        auto destroyMap = reinterpret_cast<DestroyGameMapFunc>(GetDllSymbol(dll, "DestroyGameMap"));
        if ((createMap == nullptr) || (destroyMap == nullptr)) {
            std::cerr << "Failed to find CreateGameMap/DestroyGameMap symbols" << '\n';
            UnloadDll(dll);
            return 1;
        }

        // 4. Create map via DLL factory
        GameMap* map = createMap();
        if (map != nullptr) {
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
        std::cout << "Unloaded successfully." << '\n';
    }
    
    std::cout << "\nSmoke Test Passed: 50 iterations complete with no crash." << '\n';
    return 0;
}
