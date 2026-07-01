# Implementation Plan: Project Management & Workspace System
### RayWaves v0.6.0 — "Multi-Project IDE Mode"

This plan transforms RayWaves from a **single hard-wired folder** engine into a proper multi-project workspace,
while fully preserving the **"code-first, no-ECS, raw raylib"** philosophy.

> [!IMPORTANT]
> This plan has been **grounded against the actual codebase** (as of the `main` branch after the ZigCompiler squash merge).
> Every class, file, and method reference below maps to real existing code, not hypothetical stubs.

---

## Codebase Baseline (What Already Exists)

| Concern | Existing Component | Location |
|---|---|---|
| DLL load / unload / shadow-copy | `DllLoader` (`LoadDll`, `UnloadDll`, `GetDllSymbol`) | `Game/DllLoader.h/.cpp` |
| Cross-DLL shared state | `StateBag` (float/int/bool/string/Vector2 maps) | `Engine/GameState.h` |
| Game loop & window | `GameEngine` | `Engine/GameEngine.h/.cpp` |
| Map / scene registration | `MapManager` (template `RegisterMap<T>`) | `Engine/MapManager.h/.cpp` |
| Window / scene config | `GameConfig` (INI-backed singleton) | `Engine/GameConfig.h/.cpp` |
| Editor orchestrator | `GameEditor` | `Editor/GameEditor.h/.cpp` |
| Build invocation | `ProcessRunner` (wraps `CreateProcess`) | `Editor/ProcessRunner.h/.cpp` |
| Per-editor prefs | `EditorPreferences` (singleton, file-backed) | `Editor/EditorPreferences.h/.cpp` |
| Native file dialog | `tinyfiledialogs` | `Editor/tinyfiledialogs/` |
| Compiler script | `build_gamelogic.bat` (Zig c++ invocation) | `Distribution/build_gamelogic.bat` |
| Auto hot-reload | File timestamp polling every 0.5s in `GameEditor::Run()` | `Editor/GameEditor.cpp:191` |

> [!NOTE]
> There is **no `AssetManager` class** in the codebase today. Phase 5 must create one from scratch, not extend an existing one.

---

## Phase 0 — Project Manifest & Folder Structure
**Goal:** Define the on-disk format and C++ data model. No UI, no logic.

### Files to Create
- `Engine/Project.h` — Plain struct + load/save
- `Engine/Project.cpp` — INI parsing using the **exact same `std::fstream`/`std::getline` pattern** already used by `GameConfig` and `EditorPreferences`. Zero new infrastructure.

### `project.raywaves` schema (INI)
```ini
[project]
name         = MyGame
version      = 1.0.0
engineVersion = 0.6.0
sourceDir    = GameLogic
assetDir     = Assets
entryDll     = GameLogic.dll

[editor]
cameraX    = 0.0
cameraY    = 0.0
lastMapId  =
sceneWidth  = 1280
sceneHeight = 720
targetFPS   = 60
```

### `Project` C++ struct
```cpp
struct Project {
    std::string rootPath;     // absolute, normalized
    std::string name;
    std::string version;
    std::string engineVersion;
    std::string sourcePath;   // rootPath / sourceDir
    std::string assetPath;    // rootPath / assetDir
    std::string dllPath;      // rootPath / entryDll

    // Editor state (saved on Ctrl+S)
    float cameraX = 0.f, cameraY = 0.f;
    std::string lastMapId;
    int sceneWidth = 1280, sceneHeight = 720, targetFPS = 60;

    bool LoadFromFile(const std::string_view manifestPath);
    bool SaveToFile() const;
    bool IsValid() const;       // checks all paths exist
    static bool IsProjectFolder(const std::string_view folder); // manifest present?
};
```

> [!IMPORTANT]
> **Use INI, not JSON.** `GameConfig` and `EditorPreferences` both use hand-rolled INI parsing with `std::fstream`/`std::getline` — no external library, no new pattern, no new dependency.
> `Project.cpp` reuses the exact same approach: read lines, split on `=`, check section headers with `[brackets]`, skip `#` comments.
> This is a deliberate consistency choice across all engine config files.

### `dist/Templates/` directory
```
dist/Templates/
  Empty/
    project.raywaves          ← INI manifest (name = NewProject)
    GameLogic/
      RootManager.cpp         ← minimal stub (Init/Update/Draw/Destroy)
    Assets/
      .gitkeep
  Platformer2D/
    project.raywaves          ← INI manifest (name = Platformer2D)
    GameLogic/
      RootManager.cpp         ← demo player, camera, basic physics stubs
    Assets/
      .gitkeep
```

### Deliverable
- Engine can `LoadFromFile` / `SaveToFile` a `project.raywaves` INI file.
- Template folders exist in `dist/`.
- Parsing is done with the same pattern as `GameConfig::m_bLoadFromFile()` — readable, debuggable, no surprises.

---

## Phase 1 — ProjectManager Core
**Goal:** Centralise open/close/create/recent operations. No UI yet.

### File
- `Engine/ProjectManager.h/.cpp` — Static class (no singleton; use static methods + one static `Project s_Current`)
- Recent list stored at `%APPDATA%\RayWaves\recent.ini` — same directory and format as `preferences.ini`

### API
```cpp
class ProjectManager {
public:
    static bool        OpenProject(const std::string_view folderPath);
    static void        CloseProject();                          // does NOT unload DLL — caller's job
    static bool        CreateProject(const std::string_view targetFolder, const std::string_view templateName = "Empty");
    static bool        SaveCurrentProject();

    static const Project& GetCurrent();
    static bool            HasOpenProject();

    static void                      AddRecent(const std::string_view path);
    static std::vector<std::string>  GetRecent();               // max 10, deduped

private:
    static Project                  s_Current;
    static bool                     s_bOpen;
    static std::string              s_RecentPath;  // %APPDATA%/RayWaves/recent.ini
};
```

### Key constraints grounded in real code
- **`CloseProject()` must NOT call `UnloadDll()`**. The DLL lifecycle lives in `GameEditor`; `ProjectManager` is data-only. This avoids double-unload bugs.
- `s_RecentPath` resolves to `%APPDATA%\RayWaves\recent.ini`. Same directory as `preferences.ini` — consistent.
- `recent.ini` format: simple flat list `path0=C:\...`, `path1=C:\...` up to 10 entries. Identical parse pattern to all other INI files.
- `CreateProject()` copies the template folder using `std::filesystem::copy()` recursively, then rewrites the `name` field in the copied `project.raywaves`.

### CLI integration
`Game/main.cpp` already exists and is minimal. Add:
```cpp
// main.cpp — after init, before GameEditor::Run()
if (argc >= 3 && std::string(argv[1]) == "--project")
    ProjectManager::OpenProject(argv[2]);
```

### Deliverable
- `ProjectManager::OpenProject("C:/MyGames/Platformer")` sets `s_Current` correctly.
- Recent list persists across sessions.

---

## Phase 2 — In-Engine Build System
**Goal:** `GameEditor` compiles any project's GameLogic DLL directly, no external bat scripts at runtime.

### Key insight from real code
`GameEditor::CompileGameLogic()` currently exists and calls `ProcessRunner` to run `build_gamelogic.bat`. The build bat does:
```bat
zig c++ -shared -o GameLogic.dll [src files] -ICore\Engine -ICore\raylib\include -LCore\raylib\lib -lraylib -ldwmapi -std=c++23 -msse4.2 -O2
```

Phase 2 is: **move this command-building logic into C++**, parameterised by the open `Project`.

### `GameEditor::CompileGameLogic()` refactor
Replace bat invocation with a C++ function that builds the command string dynamically:

```
zig_path     = [exe_dir]/Core/Tools/zig/zig.exe  (fallback: system zig)
src_files    = glob(project.sourcePath / "*.cpp") + glob([exe_dir]/Core/Engine/*.cpp)
output_dll   = project.dllPath
include_dirs = [exe_dir]/Core/Engine, [exe_dir]/Core/raylib/include
lib_dirs     = [exe_dir]/Core/raylib/lib
```

Then pass that single command string to the **existing** `ProcessRunner`, capturing stdout/stderr into the **existing** `BuildMessages` / `MessageLogPanel` pipeline. No new infrastructure needed.

> [!WARNING]
> The output DLL **must not** overwrite the currently-loaded shadow copy.
> The existing `DllLoader` shadow-copy mechanism already handles this correctly — `UnloadDll()` deletes the shadow copy from `%TEMP%`, and `LoadDll()` creates a new one. Just ensure `CompileGameLogic()` writes to `project.dllPath` (the original), never to the shadow path.

### Additional robustness
- Before building, validate `zig_path` exists. If not, show a `MessageLogPanel` error: `"Zig compiler not found at [path]. Re-run with -IncludeCompiler flag or install Zig system-wide."`.
- Build on a background thread (already done via `std::thread` + `b_IsCompiling` atomic in `GameEditor`). No change needed there.

### Deliverable
- Deleting `build_gamelogic.bat` from `dist/` is optional but the engine no longer depends on it at runtime.
- Any project folder compiles correctly via the in-editor "Compile" button.

---

## Phase 3 — Project Browser / Welcome Screen
**Goal:** When no project is open at startup, show a clean project picker before entering the main editor loop.

### Startup flow
```
main.cpp
  │
  ├─ --project flag? ──YES──► ProjectManager::OpenProject() → skip browser
  │
  └─NO──► GameEditor::RunBrowser()   ← NEW method
               │
               ├─ Show browser UI loop (raylib + ImGui)
               │     - Recent Projects list
               │     - "Open Project" → tinyfd_selectFolderDialog() → OpenProject()
               │     - "New Project"  → modal (Phase 6 wizard) → CreateProject() → OpenProject()
               │     - Engine logo + version string (GameEditor::version already exists)
               │
               └─ When project selected → break out of browser loop → GameEditor::Run()
```

### Why a separate `RunBrowser()` not a separate window
The engine uses raylib + ImGui initialized in `GameEditor::Init()`. The browser **reuses the same window** but renders a different ImGui layout. This avoids double-init / double-destroy of OpenGL context.

> [!IMPORTANT]
> `GameEditor::Init()` currently hardcodes `LoadImage("Assets/EngineContent/icon.png")` relative to the working directory.
> Once multi-project mode is live, `Assets/EngineContent/` is the **engine's** asset dir (next to `game.exe`), not the project's.
> Add a helper: `GetEngineContentPath()` → path of `game.exe` parent / `Assets/EngineContent/` to make this explicit.

### Browser UI components (all ImGui)
| Widget | Action |
|---|---|
| Centered logo image | Cosmetic |
| `##recent_list` scrollable panel | Click → `ProjectManager::OpenProject()` |
| "Open Existing Project" button | `tinyfd_selectFolderDialog` → validate → open |
| "New Project" button | Opens modal (Phase 6) |
| "Open GitHub / Docs" button | `ShellExecuteA` to repo URL |
| Version label | `GameEditor::version` string |

### Deliverable
- Launching `game.exe` with no args → browser screen.
- Selecting a recent project → editor loads instantly.

---

## Phase 4 — Runtime Project Switching & Hot-Reload
**Goal:** Switch between projects without restarting the engine.

### New `ProjectManager::SwitchProject()` — orchestration only
```cpp
static bool SwitchProject(const std::string_view newPath);
// Calls: SaveCurrentProject() → notify GameEditor to close DLL → OpenProject(newPath) → notify GameEditor to build+load DLL
```

### `GameEditor` integration
Add `GameEditor::OpenProject(const std::string_view folderPath)`:
1. If DLL loaded: unload via existing `UnloadDll(m_GameLogicDll)`, clear `m_MapManager`, `m_GameEngine.SetMap(nullptr)`.
2. **Clear `StateBag`** — add `StateBag::Clear()` method (trivial: call `.clear()` on all four maps).
3. Call `ProjectManager::OpenProject(folderPath)`.
4. Set `m_GameLogicPath` to `ProjectManager::GetCurrent().dllPath`.
5. Call `CompileGameLogic()` (async).
6. On compile success callback: call existing `b_LoadGameLogic()`.
7. Set `GameConfig` scene dimensions from `Project::sceneWidth/Height/targetFPS`.

### DLL safety invariant (already in place)
`GameEditor::~GameEditor()` already destroys `m_MapManager` and calls `GameEngine::SetMap(nullptr)` **before** `UnloadDll()`. `OpenProject()` must follow the same destruction order.

### Editor UI change
- `MainMenuBar.cpp`: add **"File → Switch Project..."** menu item → calls `tinyfd_selectFolderDialog` → `GameEditor::OpenProject()`.

### Deliverable
- Open Project A, play it. File → Switch Project → open Project B. Old DLL gone, new DLL compiles and loads. No restart.

---

## Phase 5 — Per-Project Asset Resolution
**Goal:** `LoadTexture("player.png")` resolves relative to the **open project's** asset folder, not CWD.

### Design: Engine-side `AssetResolver` (not a heavy AssetManager)
Since RayWaves deliberately exposes raw raylib, we do **not** wrap `LoadTexture`.
Instead, provide a path resolution helper:

```cpp
// Engine/AssetResolver.h
class AssetResolver {
public:
    static void        SetProjectAssetPath(const std::string_view path);
    static std::string Resolve(const std::string_view relativePath); // returns absolute path
    static std::string GetProjectAssetPath();
private:
    static std::string s_BasePath;
};
```

Users write:
```cpp
LoadTexture(AssetResolver::Resolve("player.png").c_str());
```

Document this in `DEVELOPER_GUIDE.md` and in the `RootManager.cpp` template stub.

### When project opens
`GameEditor::OpenProject()` calls:
```cpp
AssetResolver::SetProjectAssetPath(ProjectManager::GetCurrent().assetPath);
```

### Hot-reload compatibility
`AssetResolver` lives in the engine (compiled into `game.exe`), not the DLL. Path persists across DLL reloads. ✓

### Deliverable
- Project A's `Assets/` and Project B's `Assets/` are fully isolated.
- No path hardcoding in GameLogic source.

---

## Phase 6 — New Project Wizard
**Goal:** "New Project" modal with Name + Location + Template, opening in three clicks.

### Modal flow
```
[New Project]
  ├─ Project Name:    [text input]
  ├─ Location:        [text input] [Browse...]  → tinyfd_selectFolderDialog
  ├─ Template:        [combo box]  Empty / Platformer2D / ...
  │
  └─ [Create]  →  ProjectManager::CreateProject(location/name, template)
                →  ProjectManager::OpenProject(location/name)
                →  CompileGameLogic() (async)
```

### Template discovery
`ProjectManager::GetAvailableTemplates()` scans `[exe_dir]/dist/Templates/` using `std::filesystem::directory_iterator`. Returns `std::vector<std::string>` of folder names. The combo box is populated from this list dynamically — adding a new template folder is enough to make it appear.

### Deliverable
- Three clicks → new project folder created, manifest written, first compile triggered, editor ready.

---

## Phase 7 — Save Project (Ctrl+S)
**Goal:** Ctrl+S writes the manifest + current editor view state.

### What gets saved
The `[editor]` section of `project.raywaves` INI is rewritten on save:
- `cameraX`, `cameraY` — from `StateBag::GetVector2("camera_pos")` (if your GameLogic stores it there) — or from a new `GameEditor::GetEditorCameraPos()` helper.
- `lastMapId` — from `MapManager::GetCurrentMapId()`.
- `sceneWidth`, `sceneHeight`, `targetFPS` — from `GameEditor::m_SceneSettings`.

### Trigger
- `MainMenuBar.cpp`: handle `ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)` → call `ProjectManager::SaveCurrentProject()`.
- Optionally trigger a hot-reload compile (already available via `CompileGameLogic()`).

> [!NOTE]
> Ctrl+S should save **even if compile fails**. Saving the manifest and triggering a compile are separate operations.

### Deliverable
- Reopen a project → camera position, last active map, and scene settings are restored.

---

## Architecture Diagram

```
game.exe (main.cpp)
  │
  ├─ [no --project] ──► GameEditor::RunBrowser()
  │                         └─ ProjectManager::OpenProject()
  │                               └─ GameEditor::OpenProject()
  │
  └─ [--project path] ──► ProjectManager::OpenProject()
                              └─ GameEditor::OpenProject()
                                    ├─ UnloadDll()            [DllLoader]
                                    ├─ StateBag::Clear()
                                    ├─ AssetResolver::SetProjectAssetPath()
                                    ├─ GameConfig (scene dims)
                                    └─ CompileGameLogic() ──► b_LoadGameLogic()
                                                                   └─ LoadDll()  [DllLoader]
```

---

## Phase Dependencies & Effort

| Phase | Depends On | New Files | Touches Existing | Effort |
|---|---|---|---|---|
| **0** — Manifest + Project struct | — | `Engine/Project.h/.cpp` | — | 1 day |
| **1** — ProjectManager | Phase 0 | `Engine/ProjectManager.h/.cpp` | `Game/main.cpp` | 2 days |
| **2** — In-engine build | Phase 0, 1 | — | `Editor/GameEditor.cpp` (CompileGameLogic) | 1.5 days |
| **3** — Browser UI | Phase 1 | — | `Editor/GameEditor.h/.cpp`, `Editor/Panels/MainMenuBar.cpp` | 2.5 days |
| **4** — Runtime switch | Phase 1, 2, 3 | — | `GameEditor`, `StateBag`, `MainMenuBar` | 2 days |
| **5** — Asset resolver | Phase 0 | `Engine/AssetResolver.h/.cpp` | `Editor/GameEditor.cpp` (OpenProject) | 1 day |
| **6** — Project wizard | Phase 1, 3 | — | Browser UI modal | 1.5 days |
| **7** — Save (Ctrl+S) | Phase 1, 4, 5 | — | `ProjectManager`, `MainMenuBar` | 1 day |

**Total: ~12.5 working days** (Phases 5 and 6 can run in parallel with 3/4).

---

## Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| DLL destroyed after `FreeLibrary` (vtable gone) | 🔴 Critical | Already mitigated: `GameEditor::~GameEditor` destroys `m_MapManager` before `UnloadDll`. `OpenProject` must follow identical order. Add an assertion. |
| `std::string`/`std::unordered_map` across DLL boundary (CRT mismatch) | 🔴 Critical | `GameState.h` documents this. Zig must be invoked with `-lc++ -D_DLL` equivalent. Verify Zig flags match MSVC `/MD` CRT. Existing `build_gamelogic.bat` works — replicate its flags exactly. |
| Shadow-copy accumulation in `%TEMP%` on crashes | 🟡 Medium | `DllLoader` already cleans up on `UnloadDll`. Add a startup sweep: delete `GameLogic.shadow.*` files older than 1 hour in `%TEMP%` on engine init. |
| `tinyfiledialogs` blocking the main thread | 🟡 Medium | Already the case in `ExportPanel`. It's acceptable since dialogs are modal. No change needed. |
| Template folder not found in distribution | 🟡 Medium | `ProjectManager::GetAvailableTemplates()` must log a clear error and still allow "Open Existing Project" if `Templates/` is missing. |
| `AssetResolver` not used by GameLogic (user forgets) | 🟢 Low | Document in `DEVELOPER_GUIDE.md`. Add a warning to `MessageLogPanel` if `assetDir` resolves to a path that does not exist. |
| INI field missing in manifest | 🟢 Low | Always default-construct `Project` fields before parsing. Missing keys = use default. Never hard-fail on optional fields. |

---

## Testing Checklist

### Automated
- `Project::LoadFromFile` / `SaveToFile` round-trip (write a `Tests/` smoke-test script).
- `ProjectManager::GetRecent()` deduplication.
- `AssetResolver::Resolve()` with absolute vs relative input.

### Manual Scenarios
1. **No args** → browser screen appears with recent list.
2. **Open project** from browser → compile runs → DLL loads → game plays.
3. **File → Switch Project** → old DLL unloaded, new project compiles, loads correctly.
4. **Ctrl+S** → manifest updated, reopen same project → camera + last map restored.
5. **New Project → Empty** → folder created, first compile succeeds, blank RootManager runs.
6. **New Project → Platformer2D** → template copied, player moves.
7. **--project flag** → skips browser, loads directly.
8. **Missing Zig** → clear error in MessageLogPanel, no crash.
9. **Invalid project folder** (no manifest) → `ProjectManager::OpenProject` returns false, error logged.
10. **Switch project during compile** → second compile is queued or blocked (use `b_IsCompiling` atomic).

---

## Files Not in the Original Plan (Now Added)

| File | Reason |
|---|---|
| `Engine/AssetResolver.h/.cpp` | No `AssetManager` exists; need a lightweight resolver |
| `Tests/` smoke scripts for Project round-trip | Explicit test coverage |
| `Engine/Project.h/.cpp` parser notes | Clarifies no new JSON lib needed |
| `StateBag::Clear()` | Required by Phase 4 for safe project switch |
| `GetEngineContentPath()` helper | Required since `Assets/EngineContent/` is engine-owned, not project-owned |
| Shadow-copy startup sweep | Robustness against crash leftovers in `%TEMP%` |

---

## Final Notes

This plan preserves the **"no wrapper, raw raylib"** principle at every layer:
- `AssetResolver` is a **path helper**, not an asset cache or lifecycle manager.
- `ProjectManager` is **data-only** — it knows nothing about DLLs or raylib.
- The user's `GameLogic` still writes pure raylib code; only `AssetResolver::Resolve()` needs adding.
- All new code in the engine compiles into `game.exe`, never into `GameLogic.dll`.

**Recommended start:** Phase 0 + Phase 1 together — they are pure C++ data structures with no UI dependencies, testable immediately, and unlock every subsequent phase.
