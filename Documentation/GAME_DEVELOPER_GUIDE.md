# User Guide (For Game Developers)

*How to make games with RayWaves*

> **Engine maintainer?** See [DISTRIBUTION_GUIDE.md](DISTRIBUTION_GUIDE.md) for packaging the editor itself. This guide is for end users who just want to make games.

So you've downloaded RayWaves, or maybe your team lead sent you a zip file. Ready to make a game?

---

## 1-Minute Quick-Start

1.  Launch `RayWaves.exe`. The Project Browser appears.
2.  Click **New Project**, pick a name and location, choose a template.
3.  Click **Open Existing Project** if you want to open an existing `.raywaves` project.
4.  Once a project is open, the editor loads. Edit any `.cpp` file in your project's `GameLogic/` folder.
5.  Click **Compile** (or press the toolbar button). The build runs automatically — no manual script needed.
6.  Changes hot-reload in ~0.5 seconds. No restarting.

> **Tip:** You can also double-click a `project.raywaves` file in Explorer (after registering the file association under *Tools → Register .raywaves file association*) to skip the browser and open a project directly.

---

## The Workflow

1.  **Code** in C++ (edit your project's `GameLogic/` files).
2.  **Build** by clicking **Compile** in the editor toolbar.
3.  **Play** — the DLL swaps instantly.

You never need to close the game window to change code.

---

## Creating Your First Level

1.  Inside your project's `GameLogic/` folder, create `MyLevel.h` and `MyLevel.cpp`.
2.  Define a class inheriting `GameMap`.
3.  Register it in `RootManager.cpp`:
    ```cpp
    #include "MyLevel.h"
    // In CreateGameMap:
    s_GameMapManager->RegisterMap<MyLevel>("MyCoolLevel");
    ```
4.  Click **Compile**.
5.  Select your new level from the dropdown in the editor.

---

## Adding Assets

Put your files in your project's `Assets/` folder.

```cpp
Texture2D myTexture = LoadTexture("Assets/my_image.png");
```

Always use forward slashes (`/`), even on Windows.

---

## Sharing Your Game

1.  Open the **Export Panel** (toolbar icon).
2.  Choose output folder.
3.  Click **Start Export**.

You get a standalone `.exe` that launches your game directly (no editor UI).

---

## Need Help?

- **Game crashed?** Check the Console output in the editor.
- **Compile failed?** Look at the error messages in the Console / Message Log.
- **Weird state?** Press the **Restart** button to reset the map.
- **First compile slow?** The tools (Zig, Ninja, CMake) download automatically on first use — about 200 MB total. Internet connection required. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

*Happy Coding!*
