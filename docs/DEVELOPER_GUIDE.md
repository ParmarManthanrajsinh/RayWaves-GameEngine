# RayWaves Engine — Developer Guide

This guide covers engine internals, hot-reload mechanics, and the development workflow. For the engine repo / project folder layout, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Architecture Overview

`RayWaves.exe` (The Host) | `GameLogic.dll` (The Brains)
---|---
Handles window creation & input | Contains all gameplay code
Manages the editor UI (ImGui) | Defines levels (`GameMaps`)
Loads/unloads the DLL | Executes `Update()` and `Draw()`
**Requires restart to change** | **Hot-reloads instantly**

---

## First-Time Compile

Zig, Ninja, and CMake are automatically downloaded the first time you click Compile (or Export) if they aren't already in `Tools/`.

- **What happens:** `Tools/setup_zig.ps1` runs behind the scenes, fetches each tool, and extracts them into `Tools/{zig,ninja,cmake}/`.
- **Internet required:** First compile will block while downloading (~200 MB total across all three toolchains). A log line `"Downloading CMake (first-time setup)..."` appears in the Console so you know it isn't frozen.
- **Subsequent compiles:** No download — tools are cached. Compile time is back to normal.
- **If download fails:** Check firewall / antivirus isn't blocking `curl.exe` calls to GitHub and ziglang.org. Run `Tools/setup_zig.ps1` manually to see error details, then retry.

---

## The Hot-Reload Workflow

1.  Run the editor (`RayWaves.exe`).
2.  Open or create a project via the Project Browser.
3.  Modify any C++ file in **your project's** `GameLogic/` folder (e.g. change jump height in a map).
4.  Click **Compile** in the editor toolbar (or press the shortcut). The editor runs a CMake+Ninja build in the background.
5.  On success, the DLL is hot-swapped in ~0.5 seconds. Your changes are live without restarting.

### How it works under the hood
- Windows locks running DLLs, so we can't just overwrite them.
- **Solution:** We copy `GameLogic.dll` to a shadow file and load that. The original stays unlocked for the compiler to overwrite.
- A file watcher detects the new timestamp and triggers the reload sequence.

> **Tip:** Press the **Restart** button in the toolbar if you want to force a clean map state.

### Preserving State Across Reloads
By default, member variables reset every reload. To preserve state (player position, health, etc.), override `SaveState`/`LoadState`. Anything not explicitly saved is discarded.

```cpp
void Player::SaveState(StateBag& out) const {
    out.SetVector2("player_pos", m_Position);
    out.SetBool("player_facing_right", m_bFacingRight);
}

void Player::LoadState(const StateBag& in) {
    m_Position = in.GetVector2("player_pos", m_Position);
    m_bFacingRight = in.GetBool("player_facing_right", m_bFacingRight);
}
```

---

## Opening Projects

### Project Browser
On launch (or after closing a project), the **Project Browser** shows:
- **Recent Projects** list (left column) — click any entry to reopen.
- **New Project** wizard (right column) — picks a template and creates the folder structure.
- **Open Existing Project** — folder picker to select any folder containing `project.raywaves`.

### Double-Click File Association
If you register the `.raywaves` file association (menu: *Tools → Register .raywaves file association*), you can:
- Double-click any `project.raywaves` file in Explorer to launch the editor directly into that project.
- If the editor is already running elsewhere, a **second instance** opens (not a tab in the existing window — see limitations below).

The menu item shows a checkmark when the association is already registered and matches the current exe path. Re-register after moving `RayWaves.exe` to a new location.

### Command Line
```powershell
RayWaves.exe --project "C:\path\to\project"
RayWaves.exe "C:\path\to\project\project.raywaves"
```

Both forms work. The second is what happens when you double-click a registered `.raywaves` file.

---

## Map Development

Levels in RayWaves are C++ classes. No JSON, no proprietary editors.

### Basic Pattern

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
- **Use `delta_time`:** Multiply movement by `delta_time` for framerate-independent motion.
- **Keep it Clean:** Logic in `Update()`, rendering in `Draw()`, loading in `Initialize()`.
- **State:** Static variables persist across reloads; member variables reset.

### Asset Resolution
Use `AssetResolver` for portable paths to your project's `Assets/`:

```cpp
#include "AssetResolver.h"
Texture2D tex = LoadTexture(AssetResolver::Resolve("player.png").c_str());
```

Hardcoded `"Assets/player.png"` will break if the project root changes.

---

## Distribution Logic

When you export your game via the Export panel:

1.  **Build:** GameLogic is compiled in Release mode.
2.  **Bundle:** `GameLogic.dll`, `raylib.dll`, `game.exe`, and `Assets/` are copied to the output folder.
3.  **Configure:** A production-ready `config.ini` is generated.
4.  **Result:** A standalone folder with no editor overhead.

See [GAME_DEVELOPER_GUIDE.md](GAME_DEVELOPER_GUIDE.md) for end-user instructions, or [distribution.md](distribution.md) for engine maintainers who want to package the editor itself.

---

## Testing

Unit tests use the doctest framework.

### Running Tests
```powershell
scripts/test.ps1                 # unit + integration + smoke
scripts/test.ps1 -Suite unit     # fast engine tests
ctest --preset zig-debug         # CTest directly
```

See [testing.md](testing.md) for suite details.

### Adding a New Test
1. Create `tests/unit/MyModule_t.cpp` (or `tests/integration/` when it touches
   DLLs or the filesystem):
```cpp
#include "doctest/doctest.h"
#include "Engine/MyModule.h"

TEST_CASE("MyModule: does thing")
{
    CHECK(some_function() == expected);
}
```
2. Add the file to the matching source list in `tests/CMakeLists.txt`.
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
| Benchmarks | `benchmarks/` | 5 | Done |
| Smoke (DLL stress) | `tests/smoke/HotReloadSmoke.cpp` | configurable load/unload cycles | Done |

Tests live in `tests/unit` and `tests/integration`; benchmarks are a separate executable. Run everything with `scripts/test.ps1`.

---

## Profiling

Toggle the **Performance Overlay** in the editor toolbar to see FPS, frame times, and per-system breakdown.

### Distribution Build (Strip Profiler)
```powershell
cmake --preset zig-release -DRAYWAVES_DISTRIBUTION_BUILD=ON
cmake --build build/zig-release
```
When enabled, `SCOPED_TIMER` becomes a no-op and `PerformanceOverlay` renders an empty breakdown.

### CSV Export (Dev Only)
```cpp
Profiler::Get().SaveToFile("profile.csv");
```

---

## Current Limitations

- **Single project per window:** RayWaves opens one project at a time. Switching projects closes the current one. Tabbed multi-project editing is not supported.
- **No-project fallback compile:** Triggering Compile from the bare launcher (no project open) without a system-installed CMake will fail. The primary project-compile path handles this correctly — this edge case is a known gap.
- **Windows only:** RayWaves depends on Win32 APIs for DLL hot-reloading and file association. Cross-platform support is not planned.

---

## Advanced Tips

### raygui
`#include <raygui.h>` in your map code for immediate-mode UI. Do **not** `#define RAYGUI_IMPLEMENTATION` yourself.

### Customizing the Editor
1. Open `Editor/GameEditor.cpp` for the main loop, or the relevant piece:
   `Editor/GameLogicLoader.cpp` (DLL hot-reload lifecycle),
   `Editor/ThemeService.cpp` (theme rebake), `Editor/Panels/*` (editor UI).
2. Panels implement `IEditorPanel` — add a new panel file pair plus one `b_RegisterPanel<T>()` line rather than growing `GameEditor.cpp`.
3. Rebuild `RayWaves.exe` (must close the editor first).

### Debugging
- Attach any C++ debugger (VS Code, LLDB, Visual Studio) to `RayWaves.exe`.
- Breakpoints usually survive DLL hot-reload because Zig generates PDB files.

---

*Happy Coding!*
