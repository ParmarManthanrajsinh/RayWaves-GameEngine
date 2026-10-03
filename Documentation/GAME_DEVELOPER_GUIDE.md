# RayWaves Game Developer Guide

*How to make games with RayWaves*

> **Engine maintainer?** See [DISTRIBUTION_GUIDE.md](DISTRIBUTION_GUIDE.md) for packaging the editor itself. This guide is for end users who just want to make games.

---

## Quick Start

1. Launch `RayWaves`. The Project Browser appears.
2. Click **New Project**, pick a name and location, choose a template.
3. Once open, the editor loads. Edit `.cpp` files in your project's `GameLogic/` folder.
4. Click **Compile** (toolbar button). Build runs automatically.
5. Changes hot-reload in ~0.5 seconds. No restarting.

> **Tip:** Double-click a `project.raywaves` file in your file manager (after registering file association under *Tools -> Register .raywaves file association*) to skip the browser.

---

## Workflow

```
Code (GameLogic/*.cpp) -> Click Compile -> DLL hot-reloads -> Play instantly
```

You never close the game window to change code.

---

## What Survives a Hot-Reload

When you click **Compile**, the editor builds a new `GameLogic.so`, loads it
beside the old one (shadow copy, so the compiler can rewrite the original),
saves state, swaps the map, and restores state. Concretely:

**Survives (yes):**
- Anything you write in `SaveState` and read back in `LoadState` (`StateBag`
  keys: floats, ints, bools, strings, `Vector2`). This is the **only**
  supported channel for carrying game state across a reload.
- Map registrations (`RegisterMap<>` calls inside the `s_GameMapManager == nullptr`
  guard in `RootManager.cpp`) — they run once per process, not per reload.
- Anything `Initialize()` rebuilds from scratch (textures, sounds, fonts).

**Does not survive (no):**
- Raw pointers, texture/sound handles, or object references held in member
  variables — the old DLL is unloaded, so its code and statics are gone.
  Re-acquire them in `Initialize()` / `LoadState()`.
- `static` locals or globals holding gameplay state — they die with the old
  DLL. Put durable state in `StateBag` instead.
- Changes to what `SaveState` writes without a matching `LoadState` reader
  (or vice versa) — missing keys silently fall back to defaults.

**Rejected loads:**
- If the editor reports a *GameLogic ABI mismatch* (or a missing
  `GetGameLogicAbiVersion` export), your DLL was built with a different engine
  version. Just hit **Compile** again to rebuild it — old DLLs are refused
  rather than risk heap corruption across the DLL boundary.

**Weird state?** Press the **Restart** button to reset the map.

---

## Project Structure

```
MyGame/
├── project.raywaves      # Manifest (name, version, scene settings)
├── Assets/                # Your textures, sounds, fonts
├── GameLogic/             # YOUR C++ GAMEPLAY CODE
│   ├── RootManager.cpp    # DLL entry point, map registration
│   └── YourMap.cpp        # Any number of maps, entities, systems
└── .raywaves/             # Auto-managed build cache (do not touch)
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full engine repo structure.

---

## Which Template to Start From

| Template | Maps | Complexity | Use When |
|----------|------|-----------|----------|
| **Empty** | 1 inline map | Minimal | Learning, prototyping |
| **Platformer2D** | 1 map + camera + movement | Low | Need scrolling camera fast |
| **DemoGame** | Multi-map, entities, audio, parallax | Full | Reference for production game |

See [GUIDE_FUNDAMENTALS.md](GUIDE_FUNDAMENTALS.md) for understanding the DLL contract, map lifecycle, StateBag, and your first map tutorial.

See [GUIDE_REFERENCE.md](GUIDE_REFERENCE.md) for camera, input, audio, assets, export, and common patterns.

---

## Need Help?

- **Game crashed?** Check the Console output in the editor.
- **Compile failed?** Look at error messages in the Console / Message Log.
- **Weird state?** Press the **Restart** button to reset the map.
- **First compile slow?** First build runs your system `cmake`/`ninja` — no downloads, no bundled toolchain. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
