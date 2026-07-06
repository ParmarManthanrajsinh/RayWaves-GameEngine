# RayWaves Game Developer Guide

*How to make games with RayWaves*

> **Engine maintainer?** See [DISTRIBUTION_GUIDE.md](DISTRIBUTION_GUIDE.md) for packaging the editor itself. This guide is for end users who just want to make games.

---

## Quick Start

1. Launch `RayWaves.exe`. The Project Browser appears.
2. Click **New Project**, pick a name and location, choose a template.
3. Once open, the editor loads. Edit `.cpp` files in your project's `GameLogic/` folder.
4. Click **Compile** (toolbar button). Build runs automatically.
5. Changes hot-reload in ~0.5 seconds. No restarting.

> **Tip:** Double-click a `project.raywaves` file in Explorer (after registering file association under *Tools -> Register .raywaves file association*) to skip the browser.

---

## Workflow

```
Code (GameLogic/*.cpp) -> Click Compile -> DLL hot-reloads -> Play instantly
```

You never close the game window to change code.

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
- **First compile slow?** Tools download automatically on first use (~200 MB). Internet required. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
