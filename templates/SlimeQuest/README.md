# SlimeQuest Template

The full reference game. It deliberately stays on the raw `GameMap` /
`MapManager` API so both supported paths exist in the templates:

- `Simple2D` and `Platformer2D` show the `rw` facade.
- `SlimeQuest` shows the low-level API: multi-map registration, map
  transitions, per-map cameras, and custom `SaveState`/`LoadState`.

## What it shows

- Two maps (`DemoMainMenu`, `DemoLevel`) registered in `RootManager.cpp`.
- Main menu -> gameplay transition through `MapManager`.
- Player movement, attack, gravity, and tile collision (`Player`).
- Slime patrol, damage, death, and sound (`Slime`).
- Parallax background layers, tileset drawing, and sparkles (`DemoLevel`).
- Editor entities: `Player` and each `Slime` appear in the Scene panel with
  their live positions.
- Hot reload with custom state: player position/velocity and the current map
  survive a recompile.

## Controls

| Input | Action |
|---|---|
| `A` / `D` or left / right | Move |
| `W`, `Up`, or `Space` | Jump |
| `J` or left mouse | Attack |
| Click menu buttons | Start the game / quit |

## Files

| File | Purpose |
|---|---|
| `GameLogic/RootManager.cpp` | DLL exports, map registration, initial map |
| `GameLogic/DemoMainMenu.*` | Menu screen |
| `GameLogic/DemoLevel.*` | Gameplay map: tiles, parallax, entities, camera |
| `GameLogic/Player.*` | Player state, movement, attacks, tileset drawing |
| `GameLogic/Slime.*` | Enemy patrol/damage/death |
| `GameLogic/GameCamera.*` | Follow camera with bottom-only clamping |
| `Assets/` | Sprites, tileset, parallax layers, sounds |

## Why not `rw`?

`rw` is a 2D convenience layer. This template exists to prove that the raw
`GameMap` contract still works for larger games, custom cameras, and state that
does not fit an entity list. If you want the simple path, start from `Simple2D`.
