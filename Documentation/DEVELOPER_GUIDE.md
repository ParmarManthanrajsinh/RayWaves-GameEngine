# RayWaves Engine - Developer Guide

*Where code changes flow like waves 🌊*

This guide helps you understand how RayWaves is structured and how to get the most out of its features. Whether you're hacking on the engine core or building the next big platformer, start here.

---

## 🏗️ Architecture Overview

The engine is split into two distinct parts that work together:

`main.exe` (The Host) | `GameLogic.dll` (The Brains)
---|---
Handles window creation & input | Contains all gameplay code
Manages the editor UI (ImGui) | Defines levels (`GameMaps`)
Loads/unloads the DLL | Executes `Update()` and `Draw()`
**Requires restart to change** | **Hot-reloads instantly**

---

## 🔥 The Hot-Reload Workflow

Why restart when you can just keep coding?

1.  **Run the Editor** (`main.exe` or `editor.exe`).
2.  **Modify** any C++ file in `GameLogic/` (e.g. change jump height).
3.  **Compiling...** (The editor waits).
4.  **Reload!** The DLL is swapped, the map resets, and your changes are live.

### How it works under the hood
- Windows locks running DLLs, so we can't just overwrite it.
- **Solution:** We copy `GameLogic.dll` to a temp file (e.g., `GameLogic_temp.dll`) and load *that*.
- The original file stays unlocked, ready for your compiler to overwrite it.
- A file watcher detects the change and triggers the reload sequence.

> **Pro Tip:** Press the **Restart** button in the editor toolbar if you ever get stuck or want to manually force a clean slate.

---

## 🎨 Map Development

Levels in RayWaves are simple C++ classes. No messy JSON files or proprietary editors—just code.

### The Basic Pattern

Inherit from `GameMap` and override the big three:

```cpp
class MyLevel : public GameMap {
    void Initialize() override {
        // Load textures, sounds, setup variables
    }

    void Update(float delta_time) override {
        // Move things, check collisions, handle input
    }

    void Draw() override {
        // Draw things to the screen
    }
};
```

### Best Practices

- **Use `delta_time`:** Multipling movement by `delta_time` makes your game run smoothly on any framerate.
  ```cpp
  position.x += SPEED * delta_time; // Good!
  position.x += SPEED;              // Bad (runs faster on high FPS)
  ```

- **Keep it Clean:**
  - Logic goes in `Update()`.
  - Rendering goes in `Draw()`.
  - Heavy loading goes in `Initialize()`.

- **State Management:**
  - Remember that static variables persist across reloads (mostly).
  - Member variables reset every reload (giving you a fresh start).

---

## 📂 Project Structure

Where does everything live?

```
RayWaves/
├── Assets/             # Your images, audio, and fonts
├── GameLogic/          # 👈 YOUR CODE LIVES HERE
│   ├── RootManager.cpp # Registers your maps
│   ├── Level1.cpp      # Example level
│   └── Player.cpp      # Example class
├── Engine/             # Core engine headers (GameMap, Config)
├── Editor/             # Editor code (main.exe source)
└── Distribution/       # Scripts for packaging your game
```

---

## 📦 Distribution Logic

When you export your game, here's what happens:

1.  **Bundling:** The engine copies `GameLogic.dll`, `raylib.dll`, and `assets/`.
2.  **Configuring:** It generates a production `config.ini`.
3.  **Stripping:** It removes development files (like cpp sources) to keep the download small.
4.  **Result:** You get a clean folder ready to ZIP and upload to Itch.io or Steam.

---

## 🧠 Advanced Tips

### Customizing the Editor
Want to add a new tool to the editor toolbar?
1. Open `Editor/GameEditor.cpp`.
2. Find `DrawSceneWindow()`.
3. Add your standard ImGui code there.
4. Rebuild `main.exe` (requires stopping the app).

### Debugging
- **Console Logs:** The engine prints useful info to the attached console. Keep it open!
- **Visual Studio:** You can attach the VS debugger to `main.exe` to debug your DLL code. breakpoints *usually* work even after reloading!

---

*Happy Coding! 💜*