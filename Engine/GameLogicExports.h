#pragma once

// Portable exported-symbol marker for the three GameLogic entry points:
// GetGameLogicAbiVersion, CreateGameMap, DestroyGameMap.
#if defined(_WIN32)
#define RW_GAMELOGIC_EXPORT extern "C" __declspec(dllexport)
#else
#define RW_GAMELOGIC_EXPORT extern "C" __attribute__((visibility("default")))
#endif
