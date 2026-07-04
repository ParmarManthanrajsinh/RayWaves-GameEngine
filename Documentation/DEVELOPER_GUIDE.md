# RayWaves Engine - Developer Guide

*Where code changes flow like waves 🌊*

This guide helps you understand how RayWaves is structured and how to get the most out of its features. Whether you're hacking on the engine core or building the next big platformer, start here.

---

## 🏗️ Architecture Overview

The engine is split into two distinct parts that work together:

`RayWaves.exe` (The Host) | `GameLogic.dll` (The Brains)
---|---
Handles window creation & input | Contains all gameplay code
Manages the editor UI (ImGui) | Defines levels (`GameMaps`)
Loads/unloads the DLL | Executes `Update()` and `Draw()`
**Requires restart to change** | **Hot-reloads instantly**

---

## 🔥 The Hot-Reload Workflow

Why restart when you can just keep coding?

1.  **Run the Editor** (`RayWaves.exe` or `RayWaves.exe`).
2.  **Modify** any C++ file in `GameLogic/` (e.g. change jump height).
3.  **Compiling...** (The editor waits).
4.  **Reload!** The DLL is swapped, the map resets, and your changes are live.

### How it works under the hood
- Windows locks running DLLs, so we can't just overwrite it.
- **Solution:** We copy `GameLogic.dll` to a temp file (e.g., `GameLogic_temp.dll`) and load *that*.
- The original file stays unlocked, ready for your compiler to overwrite it.
- A file watcher detects the change and triggers the reload sequence.

> **Pro Tip:** Press the **Restart** button in the editor toolbar if you ever get stuck or want to manually force a clean slate.

### Preserving State Across Reloads
By default, member variables reset every time the DLL is swapped. To preserve your state (like player position or health), override the `SaveState` and `LoadState` methods. Anything not explicitly saved here will be discarded.

**Example (Saving Player Position):**
```cpp
void Player::SaveState(StateBag& out) const {
    out.SetVector2("player_pos", m_Position);
    out.SetBool("player_facing_right", m_bFacingRight);
}

void Player::LoadState(const StateBag& in) {
    // The second parameter is the default value if the key doesn't exist
    m_Position = in.GetVector2("player_pos", m_Position);
    m_bFacingRight = in.GetBool("player_facing_right", m_bFacingRight);
}
```

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

### 📁 Asset Resolution

When loading textures, audio, or fonts, **do not** hardcode relative paths or absolute paths. Use the `AssetResolver` class to ensure paths correctly resolve to the currently open project's `Assets` directory:

```cpp
#include "AssetResolver.h"

// Good! Resolves to the current project's Assets folder
Texture2D tex = LoadTexture(AssetResolver::Resolve("player.png").c_str());

// Bad! Will fail if you switch projects or run from a different CWD
Texture2D badTex = LoadTexture("Assets/player.png");
```

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
├── Engine/             # Core engine headers (GameMap, Config, Profiler)
├── Editor/             # Editor code (RayWaves.exe source)
├── Tests/
│   ├── run_all.bat       # Build → test → launch
│   ├── run_tests.bat     # Build → test only
│   ├── doctest/          # Single-header test framework
│   ├── GameConfig_t.cpp  # Unit tests
│   ├── MapManager_t.cpp
│   └── Profiler_t.cpp
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

## 🧪 Testing

The engine includes unit tests and integration tests built on the doctest framework.

### Running Tests

Build and run all tests:
```powershell
cmake --build build/zig-release --target tests
.\build\zig-release\tests.exe
```

Or via CTest (includes unit + smoke tests):
```powershell
ctest --test-dir build/zig-release
```

Quick shortcuts for development (run from project root):
```powershell
Tests\run_all.bat      # build tests → run → full build → launch editor
Tests\run_tests.bat    # build tests → run unit + smoke only
```

### Adding a New Test

1. Create `Tests/MyModule_t.cpp`:
```cpp
#include "doctest/doctest.h"
#include "../Engine/MyModule.h"

TEST_CASE("MyModule: does thing")
{
    CHECK(some_function() == expected);
}
```
2. Add the file to `CMakeLists.txt` in the `tests` executable source list.
3. Rebuild and run.

### Test Coverage

| Module | Test File | Cases | Status |
|--------|-----------|-------|--------|
| GameConfig | `GameConfig_t.cpp` | 5 | Done |
| Project | `Project_t.cpp` | 5 | Done |
| AssetResolver | `AssetResolver_t.cpp` | 3 | Done |
| StateBag | `StateBag_t.cpp` | 8 | Done |
| ProjectManager | `ProjectManager_t.cpp` | 2 | Done |
| Profiler | `Profiler_t.cpp` | 3 | Done |
| GameMap | `GameMap_t.cpp` | 5 | Done |
| MapManager | `MapManager_t.cpp` | 4 | Done |
| Perf benchmarks | `PerfBenchmark_t.cpp` | 5 | Done |
| Smoke (DLL stress) | `SmokeTest.cpp` | 50× load/unload | Done |

Total: **40 test cases**, **116 assertions**, plus **smoke test** (DLL load 50×).

---

## 📊 Profiling

The engine includes a built-in profiler for measuring per-frame execution times.

### Performance Overlay

Toggle the **Performance Overlay** in the editor toolbar (chart icon). Shows:
- FPS and average/max frame time
- **Per-system breakdown** (Update, Draw, panels) sorted by avg time
- 120-frame rolling window

The profiler records all `SCOPED_TIMER` annotations automatically — no setup needed.

### Distribution Build (Strip Profiler)

For release builds, disable the profiler completely to remove overhead:
```powershell
cmake -B build/zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build/zig-release
```

When enabled:
- `SCOPED_TIMER(name)` expands to `((void)0)` — zero runtime cost
- `Profiler` class methods become no-ops
- `PerformanceOverlay` renders an empty breakdown table
- Full build works without changes to any source file

### CSV Export (Dev Only)

The profiler can dump the 120-frame ring buffer to CSV via `Profiler::SaveToFile()`:
```cpp
Profiler::Get().SaveToFile("profile.csv");
```
This is not wired to the editor UI — call from code or a debug command.

---

## 🧠 Advanced Tips

### User Interfaces (UI)
Need menus, buttons or HUDs? RayWaves comes with `raygui` pre-compiled. Just `#include <raygui.h>` in your level and start drawing buttons in your `Draw()` function. **Do not** `#define RAYGUI_IMPLEMENTATION` yourself!

### Customizing the Editor
Want to add a new tool to the editor toolbar?
1. Open `Editor/GameEditor.cpp`.
2. Find `DrawSceneWindow()`.
3. Add your standard ImGui code there.
4. Rebuild `RayWaves.exe` (requires stopping the app).

### Debugging
- **Console Logs:** The engine prints useful info to the attached console. Keep it open!
- **Debuggers:** You can attach any C++ debugger (VS Code, LLDB, Visual Studio) to `RayWaves.exe` to debug your DLL code. Breakpoints *usually* work even after reloading since Zig generates standard PDB files on Windows!

---

*Happy Coding! 💜*