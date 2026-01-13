# User Guide (For Game Developers)

*How to make games with RayWaves 🌊*

So you've downloaded RayWaves, or maybe your team lead sent you a zip file. Ready to make a game?

This guide assumes you are working with the **Distributed** version of the engine (i.e., you have `editor.exe` and `build_gamelogic.bat`).

---

## ⚡ 1 Minute Quick-Start

1.  **Launch** `editor.exe` (or `app.exe`). You should see the example level.
   
2.  **Open** the folder in VS Code (or your favorite text editor).

3.  **Edit** `GameLogic/Level1.cpp`.
    *   Find `DrawText("Use arrow keys to move", ...)`
    *   Change it to `DrawText("I Hacked the Mainframe", ...)`
    *   Save the file.

4.  **Reload.**
    *   Double-click `build_gamelogic.bat`.
    *   *OR* if you are in VS Code terminal: run `./build_gamelogic.bat`

5.  **Look** at the game window. The text changed instantly. ✨

---

## 🎮 The Workflow

The "Loop" in RayWaves is simple:

1.  **Code** in C++.
2.  **Build** (using the script).
3.  **Play** (automatically reloaded).

You **never** need to close the game window to change code.

> **Tip:** Keep your terminal open next to the game window so you can just hit `Up Arrow` + `Enter` to rebuild quickly!

---

## 🏗️ Creating Your First Level

1.  **Copy** `Level1.cpp` and `Level1.h` to `MyLevel.cpp` and `MyLevel.h`.
2.  **Rename** the class inside to `MyLevel`.
3.  **Register it** in `GameLogic/RootManager.cpp`:

    ```cpp
    #include "MyLevel.h" // <--- Add header

    extern "C" ... CreateGameMap() {
        // ...
        s_GameMapManager->RegisterMap<MyLevel>("MyCoolLevel"); // <--- Register it
    }
    ```
4.  **Rebuild.**
5.  **Select** "MyCoolLevel" from the dropdown menu in the editor.

---

## 🎨 Adding Assets

Put your files in the `Assets/` folder.

```cpp
// In Initialize()
Texture2D myTexture = LoadTexture("Assets/my_image.png");

// In Draw()
DrawTexture(myTexture, 100, 100, WHITE);
```

**⚠️ Important:** Always use forward slashes (`/`), even on Windows. It's just safer.

---

## 📦 Sharing Your Game

When you are done:

1.  Open the **Export Panel** (Sales Chart Icon).
2.  Choose an output folder.
3.  Click **Start Export**.

You now have a standalone `.exe` that launches your game directly (no editor UI)!

---

## 🆘 Need Help?

*   **Game Crashed?** Check the console output.
*   **Code won't update?** Make sure the build script actually said "Build succeeded".
*   **Weird glitches?** Press the **Restart** button (Refresh icon) to reset the map state.

*Happy Coding! 💜*
