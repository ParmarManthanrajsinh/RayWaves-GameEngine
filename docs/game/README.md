# Game Documentation

Start here if you are making a game with RayWaves.

## First steps

1. Run the editor (`RayWaves.exe` on Windows, `RayWaves` on Linux).
2. Create a project from a template: `Empty2D`, `Simple2D` (recommended), `Platformer2D`, `ThirdPerson3D`, or `SlimeQuest`.
3. Edit gameplay code in your project's `GameLogic/` folder.
4. Click **Compile** in the editor.
5. Export when ready.

## Guides

- [Game developer guide](../GAME_DEVELOPER_GUIDE.md) — templates, hot reload, workflow.
- [Cookbook](cookbook.md) — copy-paste recipes for movement, markers, properties, audio.
- [Fundamentals](../GUIDE_FUNDAMENTALS.md) — DLL contract, maps, StateBag, first map.
- [Reference](../GUIDE_REFERENCE.md) — camera, input, audio, assets, export patterns.
- [API reference](../API_REFERENCE.md) — engine classes.
- [Troubleshooting](../TROUBLESHOOTING.md) — build/runtime/export failures.

## Project layout

```text
MyGame/
├─ project.raywaves      # Project manifest
├─ Assets/               # Flat: textures, audio, fonts, <MapId>.scene
├─ GameLogic/            # Your C++ gameplay code
│  ├─ RootManager.cpp    # rw::Game entry point: AddLevel<T>() + Start
│  └─ YourMap.cpp
└─ .raywaves/            # Generated build cache (do not edit)
```

Level files live directly in `Assets/` as `<MapId>.scene`. New imports land
flat in `Assets/`; the FileSystem panel groups files virtually by extension.

## Rules

- Gameplay code lives in the project `GameLogic/`, never in `Engine/`.
- Hot-reload preserves state only through `SaveState` / `LoadState`.
- Raw pointers, texture handles, and static gameplay state do not survive reload.
- Rebuild after an ABI mismatch; the editor rejects stale DLLs.
- Scene sprites are world-space editor props. Gameplay can read their `type`
  tags through `GameMap::GetSceneObjects()` / `rw::Level::SceneObjectsOfType()`;
  the engine still draws the sprite.
