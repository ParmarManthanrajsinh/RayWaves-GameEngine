# Game Documentation

Start here if you are making a game with RayWaves.

## First steps

1. Run `RayWaves.exe`.
2. Create a project from a template: `Empty`, `Platformer2D`, or `SlimeQuest`.
3. Edit gameplay code in your project's `GameLogic/` folder.
4. Click **Compile** in the editor.
5. Export when ready.

## Guides

- [Game developer guide](../GAME_DEVELOPER_GUIDE.md) — templates, hot reload, workflow.
- [Fundamentals](../GUIDE_FUNDAMENTALS.md) — DLL contract, maps, StateBag, first map.
- [Reference](../GUIDE_REFERENCE.md) — camera, input, audio, assets, export patterns.
- [API reference](../API_REFERENCE.md) — engine classes.
- [Troubleshooting](../TROUBLESHOOTING.md) — build/runtime/export failures.

## Project layout

```text
MyGame/
├─ project.raywaves      # Project manifest
├─ Assets/               # Textures, audio, fonts
├─ GameLogic/            # Your C++ gameplay code
│  ├─ RootManager.cpp    # Registers maps
│  └─ YourMap.cpp
└─ .raywaves/            # Generated build cache (do not edit)
```

## Rules

- Gameplay code lives in the project `GameLogic/`, never in `Engine/`.
- Hot-reload preserves state only through `SaveState` / `LoadState`.
- Raw pointers, texture handles, and static gameplay state do not survive reload.
- Rebuild after an ABI mismatch; the editor rejects stale DLLs.
