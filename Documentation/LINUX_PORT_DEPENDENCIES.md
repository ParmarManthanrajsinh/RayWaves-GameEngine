# Linux Port Plan

**Project:** RayWaves Game Engine
**Goal:** run engine, editor, hot-reload pipeline, game export on Linux. Minimal source change. Zero UI change.
**Method:** read every first-party source (`Engine/`, `Editor/`, `Game/`, `Tests/`, `Distribution/`, `Tools/`) + every build/script/packaging entry point. Audited vs commit `477dd3e0`; working-tree diff at audit time `clang-format` reflow only, no semantic change.
**Reference toolchain:** GCC 16.2.1, Clang 22.1.8, CMake 4.3.0, Ninja 1.13.2, GNU Make 4.4.1.

---

## 1. Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Compiler | System **GCC/G++**, no Zig | Zig on Windows existed only because MSVC not preinstalled. Linux ships GCC. Port Zig bootstrap = more work than delete it. |
| Language standard | **C++23**, GCC ≥ 11 (recommend ≥ 13) | See §2. Project already sets `CMAKE_CXX_STANDARD 23`. |
| Module format | `GameLogic.so` + `libraylib.so` | Direct analogue of DLL pair. Same shadow-copy + ABI-version architecture holds unchanged. |
| Export format | **tarball + generated `run.sh` / `install.sh`** + `.desktop` | Keeps zero-install promise. No new toolchain, no network dependency. |
| UI | **No changes** | Every Win32 call replaced behind existing signature. Menu items, labels, layout identical. |

### 1.1 Correction to the brief

Brief assumed "GCC only needs to be the latest version supporting C++ 20". Wrong. Sets published support statement.

**Codebase needs C++23, not C++ 20.** `CMakeLists.txt:55` + `Distribution/dist_CMakeLists.txt:7` already set `CMAKE_CXX_STANDARD 23`. 12 sites use `std::string::contains` / `std::string_view::contains`, C++23 addition:

| Site | Feature | Standard | Min GCC |
|---|---|---|---|
| `Game/DllLoader.cpp:86`, `:243` | `std::string::contains` | C++23 | 11 |
| `Editor/terminal/terminal.cpp:577` | `std::string::contains` | C++23 | 11 |
| `Editor/Panels/ExportPanel.cpp:419`, `:423`, `:427`, `:428`, `:429`, `:433`, `:434`, `:435` | `std::string::contains` | C++23 | 11 |
| `Editor/ProcessRunner.cpp:151` | `std::string_view::contains` | C++23 | 11 |
| `Engine/ProjectManager.cpp:215`, `:243` | `std::erase` on `std::vector` | C++20 | 10 |
| `Engine/ProjectManager.cpp:320`, `:332`, `:339`, `:349` | `std::ranges::replace` | C++20 | 10 |
| `Editor/Panels/PerformanceOverlay.cpp:55` | `std::ranges::sort` | C++20 | 10 |
| `Engine/MapManager.cpp:291` | `std::map::contains` | C++20 | 10 |

**GCC ≥ 11 = hard minimum. Recommend ≥ 13.** GCC 10 or older: `Editor/Panels/ExportPanel.cpp` + `Game/DllLoader.cpp` fail. C++20 downgrade touches 12 unrelated lines — standard not a porting decision.

---

## 2. Executive summary

Windows app in 5 independent layers. Port incremental. None skippable.

| # | Layer | First-party files | Blocker type |
|---|---|---|---|
| 1 | Win32 API surface | 9 | Hard compile failure |
| 2 | DLL loading + PE/CRT guard | 3 | Hard compile failure |
| 3 | Shell, registry, window decoration | 4 | Hard compile failure |
| 4 | Toolchain, build, packaging | 17 | Silent wrong behaviour — dangerous layer |
| 5 | Path, suffix, command conventions | ~20 | Silent wrong behaviour |

Totals: **11 unguarded Win32 header inclusions**, **~90 Win32 call sites**, **~40 hardcoded Windows filename literals**, **14 Windows-only scripts/binaries**, **10 `__declspec(dllexport)` sites**, **3 vendored libraries already portable**.

**Core engine clean.** `Engine/GameMap`, `Engine/MapManager`, `Engine/AssetResolver`, `Engine/Profiler`, `Engine/GameConfig`, `Engine/raygui_impl` hold zero Windows dependency. Whole `Engine/` platform surface = `Engine/WindowUtils.cpp` (67 lines), `Engine/ProjectManager.cpp` (4 sites), comments in `Engine/GameState.h`.

### 2.1 Blocking defect, platform-neutral

`Editor/GameEditor.cpp:895` calls `GetApplicationDirectory()`. Identifier **not declared or defined anywhere in repo** — verified vs `HEAD` + working tree. `main` target fails on every platform. No green build to port from. Fix first; rest assumes fixed.

Replacement: `std::filesystem::path(GetHostExePath()).parent_path()`, using `Game/DllLoader.h:49`.

---

## 3. Layer 1 — Win32 API surface

### 3.1 Unguarded header inclusions

All unconditional. Each = hard error under Linux compiler.

| File | Line | Header |
|---|---|---|
| `Game/DllLoader.cpp` | 2 | `<Windows.h>` — capital `W`; fails on case-sensitive FS even with shim |
| `Game/PeCrtCheck.cpp` | 2 | `<Windows.h>` — same |
| `Game/main.cpp` | 4 | `<crtdbg.h>` |
| `Editor/EditorUtils.cpp` | 3, 4 | `<windows.h>`, `<shellapi.h>` |
| `Editor/FileAssociation.cpp` | 2, 3 | `<windows.h>`, `<shlobj.h>` |
| `Editor/ProcessRunner.cpp` | 9, 10 | `#define WIN32_LEAN_AND_MEAN`, `<windows.h>` |
| `Editor/terminal/terminal.cpp` | 14 | `<crtdbg.h>` (plus `CRTDBG_MAP_ALLOC` at 12) |
| `Engine/ProjectManager.cpp` | 6, 7 | `<windows.h>`, `<shlobj.h>` |
| `Engine/WindowUtils.cpp` | 8, 9, 14 | `<windows.h>`, `<dwmapi.h>`, `<shellapi.h>` |

Two MSVC-only constructs, non-conditional:

| File | Line | Construct | Problem |
|---|---|---|---|
| `Game/DllLoader.h` | 15 | `#define WIN32_LEAN_AND_MEAN` | Placed after all includes, before none. Inert even on Windows; leaks macro into every TU. |
| `Engine/WindowUtils.cpp` | 15, 16 | `#pragma comment(lib, "Shell32.lib")` / `"Dwmapi.lib"` | MSVC-only; ignored by GCC/Clang. File cannot link under non-MSVC today. |

### 3.2 Win32 call sites by category

**Executable path** — `GetModuleFileNameA` + `MAX_PATH`: `Game/DllLoader.cpp:36-42`, `Engine/ProjectManager.cpp:13-14`, `:323-324`, `Engine/WindowUtils.cpp:45-46`, `Editor/FileAssociation.cpp:9-10`.

**Dynamic loading** — `LoadLibraryA` at `Game/DllLoader.cpp:140`, `:201`, `:211`, `:219`; `FreeLibrary` at `:230`; `GetProcAddress` at `:272`; `GetCurrentProcessId` + `GetTickCount64` at `:186-187`.

**PE parsing** — all `Game/PeCrtCheck.cpp`. File mapping at `:25`, `:30`, `:33`, `:36`, `:50`, `:146-148`. PE structures at `:54`, `:58`, `:64-66`, `:68-79`, `:104`, `:107-109`, `:125`, `:128`.

**Shell** — `ShellExecuteW` at `Editor/EditorUtils.cpp:14` (`explore`), `:29` (`open`).

**Registry** — `Editor/FileAssociation.cpp:19-58` (write), `:63-85` (query).

**Process + pipes** — `Editor/ProcessRunner.cpp`: `HANDLE` RAII `:16-55`, `CreatePipe` `:67-74`, `STARTUPINFOA` `:87-92`, `CreateProcessA` `:100-112`, `ReadFile` `:131-144`, `WaitForSingleObject`/`GetExitCodeProcess` `:181-184`.

**Known folder** — `SHGetFolderPathA(CSIDL_APPDATA)` at `Engine/ProjectManager.cpp:99-100`.

**MSVC CRT** — `Game/main.cpp:36-42` (`_CrtSetDbgFlag`, `_CrtSetReportMode`, `_CrtSetReportFile`); `Editor/terminal/terminal.cpp:12-14` (`CRTDBG_MAP_ALLOC`).

**Non-portable CRT** (absent Linux): `_popen` at `Editor/ExportService.cpp:185`, `Editor/terminal/terminal.cpp:694`; `_pclose` at `Editor/ExportService.cpp:197`, `Editor/terminal/terminal.cpp:713`, `:729`; `strerror_s` at `Editor/terminal/terminal.cpp:701`.

**Already portable — pattern to copy** — `Editor/terminal/terminal.cpp:218-222` guards `localtime_s` with `#ifdef _WIN32`, falls back to `localtime_r`. `Editor/rlImGui/rlImGui.h:38-45` carries correct `__declspec` / `__attribute__` dual.

---

## 4. Layer 2 — DLL loading, PE parsing, CRT guard

### 4.1 What exists

| File | Responsibility |
|---|---|
| `Game/DllLoader.h` / `.cpp` | Shadow-copy load, symbol resolution, unload, stale-shadow sweep, ABI message format |
| `Game/PeCrtCheck.h` / `.cpp` | Parse module import table, detect static-vs-dynamic CRT mix |
| `Game/game.cpp:12-65` | Standalone runtime load path + ABI version check |
| `Editor/GameLogicLoader.cpp:149-289` | Editor hot-reload load path + ABI version check |

### 4.2 The shadow copy must be preserved

`Game/DllLoader.cpp:125-129` states reason: `LoadLibrary` locks file, DLL cannot overwrite while host runs. **Same problem on Linux, different reason.** `dlopen` maps `MAP_PRIVATE`; mapped executable cannot truncate in place — writer gets `ETXTBSY` ("text file busy"). Hot reload without restart = product feature. Mechanism portable; stated rationale not.

Three details correctness-relevant, must survive:

- `Game/DllLoader.cpp:176-183` random subdirectory blocks symlink pre-creation in world-writable temp. Keep.
- `Game/DllLoader.cpp:46-64` `FormatAbiMismatchMessage` pure + unit-tested. Keep unchanged.
- `dlopen` at `:201` needs **absolute** path. `LoadLibraryA` took relative; `dlopen` unreliable. Wrap in `fs::absolute()`.

One removal: retry-delete loop at `:246-253` (5 attempts, 100 ms apart) exists because `FreeLibrary` returns before mapping releases. `dlclose` synchronous — loop + sleep dead weight.

### 4.3 PeCrtCheck must be replaced, not ported

`Game/PeCrtCheck.cpp` asks one thing: host + plugin link same CRT? Windows `/MT` vs `/MD` split corrupts heap the instant `std::string` or `StateBag` crosses module boundary.

No Linux analogue. GCC/Clang have no static-CRT option; `new`/`delete` + `std::*` always resolve to one shared `libstdc++`/`libc++` at runtime. Real Linux analogue = **libstdc++ vs libc++**, or two libstdc++ versions — already documented at `Documentation/TROUBLESHOOTING.md:34-39`.

Delete file + remove from `CMakeLists.txt:112`, `:254`, `:292`, `:329`. No Linux stub: `b_CrtImportsCompatible` at `Game/PeCrtCheck.cpp:181-187` treats empty import set as "static CRT" — same output as malformed file, so failed parse indistinguishable from valid answer.

### 4.4 Module naming

`.dll` hardcoded in eight places. Each derive from suffix constant:

- `Engine/Project.cpp:26` — default `m_EntryDll = "GameLogic.dll"`
- `Distribution/Templates/{Empty,Platformer2D,SlimeQuest}/project.raywaves` line 7 — `entryDll=GameLogic.dll`
- `Game/game.cpp:93` — `LoadDll("GameLogic.dll")`
- `Game/DllLoader.cpp:88` — stale-shadow sweep filter `extension() != ".dll"`
- `Tests/SmokeTest.cpp:32` — `LoadDll("GameLogic.dll")`
- `Editor/ExportService.cpp:97`, `:204`, `:207`, `:227`
- `Tests/ExportService_t.cpp:107`

`Game/DllLoader.cpp:88` = silent failure. Linux sweep matches nothing; stale shadow copies pile up in temp forever, nothing reports it.

### 4.5 `__declspec(dllexport)`

Ten sites export GameLogic ABI:

- `Distribution/Templates/Empty/GameLogic/RootManager.cpp:8`, `:13`, `:25`
- `Distribution/Templates/Platformer2D/GameLogic/RootManager.cpp:8`, `:13`, `:25`
- `Distribution/Templates/SlimeQuest/GameLogic/RootManager.cpp:19`, `:24`, `:42`
- `Tests/DllLoader_t.cpp:34-36` + `:47-49`, **inside raw string literal** written to disk, compiled at test time

Just **delete** attribute. GCC exports `extern "C"` at default visibility. `RAYWAVES_GAMELOGIC_API` macro only if `-fvisibility=hidden` adopted later — worthwhile eventually, not required now. `Tests/DllLoader_t.cpp` sites need string content edited, not the TU.

---

## 5. Layer 3 — Shell, registry, window decoration

### 5.1 File association

`Editor/FileAssociation.cpp` writes `HKCU\Software\Classes\.raywaves` → `RayWaves.Project` with `DefaultIcon` (`:44`) + `shell\open\command` (`:54`), then `SHChangeNotify(SHCNE_ASSOCCHANGED, ...)` at `:58`. `IsRayWavesFileAssociationRegistered()` at `:63-85` reads registry back to drive menu checkmark at `Editor/Panels/MainMenuBar.cpp:86-108`.

Linux = freedesktop.org MIME DB. Structurally different: MIME defs, desktop entries, shared cache refreshed explicitly.

1. `~/.local/share/mime/packages/application-x-raywaves-project.xml` — `<mime-type>` glob `*.raywaves`
2. `~/.local/share/applications/RayWaves.desktop` — `MimeType=application/x-raywaves-project;`, `Exec=<exe> %f`, `Icon=raywaves`
3. `update-mime-database ~/.local/share/mime` + `update-desktop-database ~/.local/share/applications`
4. `IsRayWavesFileAssociationRegistered()` parses `~/.config/mimeapps.list`

Replace function bodies, not signatures. `Editor/Panels/MainMenuBar.cpp` then compiles + behaves identically.

Two notes:

- **Timing.** `SHChangeNotify` = instant on Windows. Without cache refresh, XDG change waits for re-login. Refresh synchronously so checkmark at `MainMenuBar.cpp:105-108` stays truthful.
- **Icon.** `:44` embeds `.ico`. XDG `Icon=` wants themed name or installed PNG/SVG path. `EngineContent/icon.png` = right source, also where deleted `rcedit` output now goes.

### 5.2 Shell integration

- `Editor/EditorUtils.cpp:14` `ShellExecuteW(L"explore")` → `xdg-open <path>` (or `gio open`)
- `Editor/EditorUtils.cpp:29` `ShellExecuteW(L"open")` → `xdg-open <url>`
- `Editor/EditorUtils.cpp:28` converts `std::string_view` → `std::wstring` by naive copy — URL becomes ASCII-only. Linux: pass `std::string`, drop conversion. Two callers (`Editor/GameEditor.cpp:448`, `Editor/Panels/MainMenuBar.cpp:123`) hardcoded ASCII URLs — latent bug, not active.
- `Editor/EditorUtils.cpp:13` `make_preferred()` no-op on Linux. Harmless.

**`IsShellSafe` at `Editor/EditorUtils.cpp:33-37` load-bearing after port.** Deny-list `&|;$"'<>%!^()@#\n\r` tuned for `cmd.exe`. Under `sh`, backslash = escape char, must add. Gates every project path reaching `sh -c` at `Editor/GameEditor.cpp:923`, `Editor/ExportService.cpp:183`, `Editor/terminal/terminal.cpp:691`.

### 5.3 Window decoration

| Line | Call | Linux disposition |
|---|---|---|
| `Engine/WindowUtils.cpp:20-34` | `DwmSetWindowAttribute(hwnd, 19/20, ...)` dark titlebar | **No portable equivalent** — compositor-controlled. Body → no-op. |
| `Engine/WindowUtils.cpp:36-53` | `ExtractIconA` + `WM_SETICON` | **Redundant.** `Editor/GameEditor.cpp:143-146` already calls raylib `SetWindowIcon(LoadImage(...))`, cross-platform. Delete. |
| `Engine/WindowUtils.cpp:55-66` | `SetupNativeWindow()` | Keep. Must return `true` unconditionally after stub: `Engine/GameEngine.cpp:57-60` returns early on `false`, `m_bIsRunning` stays false, window never runs loop. |

`Engine/WindowUtils.h:3-5` already documents right discipline — `void*` handles in header, Win32 headers confined to `.cpp`. Preserve.

### 5.4 Clipboard and input — no work

`Editor/rlImGui/rlImGui.cpp:121`, `:126`, `:315-318`; `Editor/Panels/MessageLogPanel.cpp:78`, `:153`, `:160`; `Editor/terminal/terminal.cpp:385`, `:472`, `:480`, `:531`, `:539`. All via raylib `GetClipboardText`/`SetClipboardText`, implemented per platform by ImGui + raylib. Nothing to do.

`tinyfiledialogs` results already null-checked at `Editor/Panels/ExportPanel.cpp:84`, `Editor/Panels/MainMenuBar.cpp:36-38`. Linux backend shells out `zenity`/`kdialog`, absent on minimal images — existing null checks handle it.

---

## 6. Layer 4 — Toolchain, build, packaging

Most files, least obvious breakage. Nothing here errors at compile time; it yields a silently wrong build.

### 6.1 Windows-only scripts and binaries

| File | Purpose | Disposition |
|---|---|---|
| `Tools/zig-cc.bat`, `Tools/zig-cxx.bat` | Set as `CMAKE_C_COMPILER` / `CXX_COMPILER` | **Delete.** Wrapper existed only to hide `zig.exe`. `gcc`/`g++` need no wrapper. |
| `Tools/setup_zig.ps1` | Downloads Zig, rcedit, Ninja, CMake | **Delete.** All four URLs = Windows artifacts: `:17` Zig windows zip, `:53` `rcedit-x64.exe`, `:66` `ninja-win.zip`, `:85` CMake windows zip. |
| `Tools/rcedit.exe` | PE icon stamping | **Delete.** Used at `CMakeLists.txt:21`, `:214`, `:263`, `Editor/ExportService.cpp:274`. No PE-resource concept on Linux. |
| `Tools/ninja/ninja.exe`, `Tools/zig/zig.exe`, `Tools/cmake/bin/cmake.exe` | Bundled toolchain | **Do not vendor.** System `make`, `cmake`, `gcc` present. |
| `Tools/run_analysis.bat` | clang-tidy / clang-format driver | Port to `.sh`. `:31` hardcodes `C:\Program Files\LLVM\bin`; `:61-71` probes only `*.exe`. |
| `Distribution/distribute.ps1` | 180-line packaging script | Port to `.sh`. |
| `Distribution/create_distribution.bat` | Distribution entry point | Deleted — `distribute.sh` + `make dist` are the entry points. |
| `Tests/run_tests.bat`, `run_all.bat`, `run_smoketest.bat` | Test runners hardcoding `build\zig-release` + `.exe` | **Delete.** Replaced by `make test` / `make smoke`. |
| `EngineContent/app.rc`, `EngineContent/icon.ico` | Win32 resource script + icon | Deleted — icons are PNG (`EngineContent/icon.png`); nothing embeds into binaries on Linux. |

Biggest simplification in port. Zero-install story changes shape: Windows bundles compiler because MSVC absent; Linux needs none — distribution shrinks. `Distribution/distribute.ps1:139-155` bundles rcedit + Ninja + CMake into dist — **do not port that block.**

One dist detail to keep: `distribute.ps1:68` renames runtime to `Core/runtime.exe`, `Editor/ExportService.cpp:146` detects dist layout by probing `Core/runtime.exe`. Keep target name `runtime`, drop only `.exe`.

### 6.2 CMake constructs requiring guards

| Location | Construct | Fix |
|---|---|---|
| `CMakeLists.txt:4-16` | Zig auto-fetch via `powershell ... setup_zig.ps1` | Delete block |
| `CMakeLists.txt:21-31` | rcedit auto-fetch with hard `FATAL_ERROR` | Guard `if(WIN32)` |
| `CMakeLists.txt:44` | `find_program(clang-tidy HINTS "C:/Program Files/LLVM/bin")` | Drop hardcoded path |
| `CMakeLists.txt:63` | `-msse4.2` unconditional | Guard on `CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64\|i.86"` |
| `CMakeLists.txt:64` | `-fno-sanitize=all` defeats ASan lines commented at `:60-61` | Make cache option |
| `CMakeLists.txt:65` | `-Qunused-arguments` Clang-only; GCC rejects | Guard by compiler ID |
| `CMakeLists.txt:72` | `BUILD_SHARED_LIBS ON` | **Keep.** `:70` says why: editor + plugin must share one raylib instance, one RLGL state |
| `CMakeLists.txt:100-105` | Stages only `libraylib.dll.a`; `.a` rename = MinGW-only | Also stage `libraylib.so` |
| `CMakeLists.txt:177` | `target_link_libraries(Engine PRIVATE dwmapi)` | Guard `if(WIN32)` |
| `CMakeLists.txt:199` | `add_executable(main WIN32 ...)` | `WIN32` = Windows-only property |
| `CMakeLists.txt:205`, `:255` | `EngineContent/app.rc` as source needs `windres` | Deleted (file gone) |
| `CMakeLists.txt:208`, `:257` | `-Wl,--subsystem,windows` rejected by Linux `ld` | Guard `if(WIN32)` |
| `CMakeLists.txt:213-215`, `:262-264` | rcedit POST_BUILD | Guard `if(WIN32)` |
| `CMakeLists.txt:351-356` | `export_package` hardcodes `distribute.ps1` | Repoint at `.sh` |

`Distribution/dist_CMakeLists.txt:12-14` (`if(MSVC) /MP`) + `:53-55` (`if(MINGW)`) already correctly guarded — no change. `:81` copies `libraylib.dll` post-build, must copy `libraylib.so`.

### 6.3 Missing rpath — new requirement

Zero rpath in tree. Windows needed none: `libraylib.dll` sits beside executable. Linux: `dlopen("GameLogic.so")` succeeds, then fails to resolve `libraylib.so`. Add:

```cmake
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set_target_properties(GameLogic PROPERTIES
    BUILD_RPATH "${RAYLIB_BUILD_DIR}")
```

### 6.4 Generated per-project CMake

`Engine/ProjectManager.cpp:306-406` writes CMake script as **text** into `<project>/.raywaves/CMakeLists.txt`. Highest-risk single function in port: mistake surfaces inside *end user's* project, not engine build.

| Line | Emitted | Linux |
|---|---|---|
| `:344` | `if(NOT EXISTS ".../zig/zig.exe")` | Delete Zig bootstrap |
| `:347`, `:363` | `COMMAND powershell -ExecutionPolicy Bypass -File setup_zig.ps1` | Delete |
| `:360`, `:372` | `ninja/ninja.exe` probe + forced `CMAKE_MAKE_PROGRAM` | Delete; use default generator |
| `:373-374` | `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER` set to `.bat` files | Omit; let CMake detect |
| `:381` | `add_compile_options(-msse4.2)` | Drop |
| `:400` | `target_link_directories(... $RAYLIB_DIR/lib)` | Needs `.so` staging from `CMakeLists.txt:100` |
| `:401` | `target_link_libraries(GameLogic PRIVATE raylib dwmapi)` | Drop `dwmapi`; add `-Wl,-rpath` |
| `:390-391` | `file(GLOB_RECURSE ...)` no `CONFIGURE_DEPENDS` | Add it; `Distribution/dist_CMakeLists.txt:32-33` gets right |

`GLOB` without `CONFIGURE_DEPENDS` = new `.cpp` invisible until CMake cache cleared. Copy `dist_CMakeLists.txt` pattern.

`:320`, `:332`, `:339`, `:349` hold `std::ranges::replace(x, '\\', '/')` normalisations. Dead on Linux; guard `#ifdef _WIN32`.

### 6.5 Presets

`CMakePresets.json:11-15` gates `windows-base` on `hostSystemName == Windows` — no preset usable on Linux. `:23-25` force `.bat` wrappers with `CMAKE_SYSTEM_NAME: Windows`, a cross-compilation setup.

Add `linux-base` without host condition, then `linux-debug` + `linux-release` inheriting, native compilers.

### 6.6 Also fix

`.clangd:2` sets `CompilationDatabase: build` — misses preset-based Linux build tree.

`.gitignore` ignores `*.exe`, `*.dsym`, `Distribution/Templates/**/*.dll`, `*.pdb`, `*.lib`, not `*.so`. Template build artifacts will show as untracked. Also ignores `*.json` globally — new JSON config untracked; `CMakePresets.json` survives only because committed before that rule.

---

## 7. Layer 5 — Path, suffix, and command conventions

Not Win32 calls, but they break the port if missed.

| Convention | Sites | Linux issue |
|---|---|---|
| `MAX_PATH` (260) | `Game/DllLoader.cpp:38`, `:150`; `Engine/ProjectManager.cpp:13`, `:99`, `:323`; `Engine/WindowUtils.cpp:45`; `Editor/FileAssociation.cpp:9` | Undefined. Use `std::filesystem::path` or `PATH_MAX` (4096) — real improvement, not cosmetic |
| `\` → `/` rewriting | `Engine/ProjectManager.cpp:320`, `:332`, `:339`, `:349` | Dead code |
| `cd /d "<path>" && ...` | `Editor/GameEditor.cpp:923`; `Editor/ExportService.cpp:183`; `Editor/terminal/terminal.cpp:691` | `/d` = `cmd.exe` drive-discard syntax; `sh` needs plain `cd` |
| `cmd.exe /C` | `Editor/ProcessRunner.cpp:95-96` | `/bin/sh -c` |
| `""…""` double-wrapping | `Editor/GameEditor.cpp:931`, `:944`; `Editor/ExportService.cpp:279` | `cmd.exe` quote-stripping workaround, documented at `ExportService.cpp:278`. Under `sh` = wrong, not redundant |
| `APPDATA` | `Editor/EditorPreferences.cpp:14` | Unset on Linux; preferences fall to CWD, appear not to persist |
| `CSIDL_APPDATA` | `Engine/ProjectManager.cpp:100` | Same resolution so preferences + `recent.ini` share one dir |
| `build/zig-release`, `build/zig-debug` | `Editor/ExportService.cpp:147-148`; `Distribution/distribute.ps1:18`; `Tests/run_*.bat`; `Tools/run_analysis.bat:32`, `:74` | `zig-` dir dies with Zig; use CMake-provided constants |
| `Tools/setup_zig.ps1` as engine-root sentinel | `Engine/ProjectManager.cpp:17`, `:25` | File absent on Linux; root detection collapses to last fallback, `Distribution/Templates` lookups fail |
| `.rc` / `rcedit` | `CMakeLists.txt:205`, `:214`, `:255`, `:263`; `Editor/ExportService.cpp:274` | No analogue |

---

## 8. Porting design: four platform headers

Goal: every existing caller, UI included, compiles untouched. `Engine/WindowUtils.h:3-5` documents right discipline; follow it.

```
Engine/Platform/PlatformModule.h    dlopen/dlsym/dlclose  vs LoadLibrary/GetProcAddress/FreeLibrary
Engine/Platform/PlatformPaths.h     /proc/self/exe, XDG dirs, ".so"/"" suffixes, PATH_MAX
Engine/Platform/PlatformDesktop.h   desktop entry + mime XML builders, shared by
                                    FileAssociation.cpp and export install.sh

(Linux-only pivot: no `#ifdef _WIN32` anywhere. The process-runner and
shell-open seams were folded straight into `Editor/ProcessRunner.cpp` and
`Editor/EditorUtils.cpp` instead of getting their own headers.)
```

### 8.1 `PlatformModule.h`

Most port value sits here — 8 blocking sites collapse to three one-line swaps.

```cpp
#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using ModuleHandle = HMODULE;
inline ModuleHandle PlatformLoadModule(const char* p) { return LoadLibraryA(p); }
inline void* PlatformGetSymbol(ModuleHandle h, const char* n)
{ return (void*)GetProcAddress(h, n); }
inline void PlatformUnloadModule(ModuleHandle h) { if (h) FreeLibrary(h); }
#else
#include <dlfcn.h>
using ModuleHandle = void*;
inline ModuleHandle PlatformLoadModule(const char* p)
{ return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
inline void* PlatformGetSymbol(ModuleHandle h, const char* n) { return dlsym(h, n); }
inline void  PlatformUnloadModule(ModuleHandle h) { if (h) dlclose(h); }
#endif
```

`RTLD_NOW` matters: surfaces missing `libraylib.so` at load time with real error string, not at first `dlsym`. `RTLD_LOCAL` stops plugin symbols leaking into global namespace, colliding with host's own Engine copy.

### 8.2 `PlatformPaths.h`

```cpp
inline std::string SharedLibrarySuffix();  // ".dll" / ".so"
inline std::string ExecutableSuffix();     // ".exe" / ""
inline fs::path HostExecutablePath();      // /proc/self/exe
inline fs::path UserConfigDir();           // XDG_CONFIG_HOME, else ~/.config
```

`UserConfigDir()` = single source for both `Editor/EditorPreferences.cpp:14` and `Engine/ProjectManager.cpp:100`.

### 8.3 Why the process runner uses `popen`, not `fork`+`execvp`

`Editor/ProcessRunner.cpp` = largest Windows-specific block in editor: `HANDLE` RAII `:16-55`, `CreatePipe` `:67-74`, `STARTUPINFOA` `:87-92`, `CreateProcessA` `:100-112`, `ReadFile` `:131-144`, `WaitForSingleObject` `:181-184`.

Faithful POSIX port ≈ 80 lines of `pipe`/`fork`/`execvp`/`waitpid`. Sole consumer = `Editor/GameEditor.cpp:951-978`, wants two things: line-by-line output + success boolean. `popen`/`pclose` gives both in ~30 lines.

Wrap body in `#ifdef _WIN32`, add `popen` branch, change `:95` from `"cmd.exe /C "` to `"/bin/sh -c"`. Caller unchanged → UI unchanged.

---

## 9. Phased plan

Phases A–E + I = actual port, all small. F + G = real work, both self-contained files.

### Phase A — Delete, do not port

| Target | Why |
|---|---|
| `Game/PeCrtCheck.cpp`, `.h` + `CMakeLists.txt:112`, `:254`, `:292`, `:329` | PE parser guarding static/dynamic CRT split that does not exist under GCC |
| `Tools/zig-cc.bat`, `Tools/zig-cxx.bat` | Wrapper's sole purpose = hiding `zig.exe` |
| `Tools/setup_zig.ps1` | Four Windows-artifact downloads |
| `CMakeLists.txt:4-16`, `:21-31` | Zig + rcedit auto-fetch |
| `Tools/rcedit.exe` references | `CMakeLists.txt:21`, `:214`, `:263`; `Editor/ExportService.cpp:274` |
| `Tests/run_tests.bat`, `run_all.bat`, `run_smoketest.bat` | Replaced by `make test` / `make smoke` |
| `Engine/GameState.h:18-22` comment | Restate: "same compiler + same libstdc++ as host" — real Linux rule |

### Phase B — CMake guards

Eleven `if(WIN32)` guards per §6.2, plus `.so` staging at `CMakeLists.txt:100-105`, rpath from §6.3, new `linux-*` presets. Fix `Editor/GameEditor.cpp:895` (§2.1) first — blocks everything.

Smallest change yielding compiling binary. Verify here before proceeding.

### Phase C — Module loader

Keep shadow-copy algorithm entirely (§4.2). Changes: header → `<dlfcn.h>`; `GetHostExePath` → `/proc/self/exe`; three loader calls → `PlatformModule.h`; pid/tick → `getpid()` / `std::chrono::steady_clock`; shadow extension filter → suffix constant; `fs::absolute()` before `dlopen`; drop retry-delete loop. Delete `__declspec(dllexport)` at ten sites (§4.5). Update three `project.raywaves` templates + `Engine/Project.cpp:26`.

### Phase D — Process execution

`Editor/ProcessRunner.cpp` per §8.3. `Editor/terminal/terminal.cpp:694`, `:713`, `:729` → `popen`/`pclose`, `:701` → `strerror_r` (returns `int`), `:691` `cd /d` → `cd`. Delete `:12-14`. Leave `:16-19` alone — `#define new DEBUG_NEW` guard already `_MSC_VER`-only; widening it changes every allocation in TU.

`Editor/GameEditor.cpp:923` + `Editor/ExportService.cpp:183` — `cd /d` → `cd`; re-derive quoting, `""…""` hack exists for `cmd.exe`, wrong under `sh`. Add `\` to `EditorUtils.cpp:35`. Reject unsafe project path at `GameEditor.cpp:902-905` *before* spawning, not by assigning `build_cmd = "echo ERROR: ..."` and running it.

`Editor/GameEditor.cpp:909`, `:935` + `Editor/ExportService.cpp:172` — use `cmake` from `PATH`.

### Phase E — Paths and config

Per §7 + `Engine/Platform/PlatformPaths.h`: `/proc/self/exe`, XDG config dir, drop `.ps1` sentinel for platform-neutral marker (reorder so existing `Distribution/Templates` probe at `Engine/ProjectManager.cpp:33` runs first), guard `\` rewrites, parameterise suffixes.

`Editor/EditorUtils.cpp` — `xdg-open` for `:14` + `:29`, drop `wstring` conversion at `:28`.

`Engine/WindowUtils.cpp` — no-op dark titlebar, delete `SetIconFromExecutable` (redundant with `Editor/GameEditor.cpp:143-146`), `SetupNativeWindow()` returns `true` unconditionally.

### Phase F — File association (XDG)

Per §5.1. Replace bodies of `RegisterRayWavesFileAssociation()` + `IsRayWavesFileAssociationRegistered()`; signatures stay. `Editor/Panels/MainMenuBar.cpp` compiles unchanged, checkmark stays truthful.

Also `Editor/Panels/ExportPanel.cpp:82`: icon picker filters `*.ico` only, Linux desktop entry wants PNG or SVG. Widen filter, widget unchanged.

### Phase G — Distribution: sh + Makefile helpers

See §10 and §11.

### Phase H — Export: tarball + `install.sh`

See §12.

### Phase I — Tests

| File | Change |
|---|---|
| `Tests/DllLoader_t.cpp:55` | `zig.exe` → `g++` from `PATH` |
| `Tests/DllLoader_t.cpp:79` | `-o TestGameLogic.dll` → `.so`; add `-Wl,-rpath,<raylib_lib>` or `dlopen` resolves nothing |
| `Tests/DllLoader_t.cpp:34-36`, `:47-49` | delete `__declspec(dllexport)` from embedded source strings |
| `Tests/ExportService_t.cpp:104-116` | `.exe`/`.dll` fixtures track new suffixes |
| `Tests/GameEditor_t.cpp:33-45` | asserts literal `C:\path\to\cmake.exe`; passes on Linux while asserting Windows behaviour. Rewrite against platform-agnostic builder |
| `Tests/SmokeTest.cpp:32` | `GameLogic.dll` → `.so` |
| `Tests/SmokeTest.cpp:22` | portable, no change |

---

## 10. Distribution: shell scripts + Makefile helpers

CMake stays the only build system. `.sh` scripts and the `Makefile` are automation helpers in the same tier — Makefile = thin alias layer over shell commands, exactly as `.sh` = thin wrapper over CMake. Neither may duplicate build logic: CMake already resolves raylib `FetchContent`, generator, staging, and a second copy of that logic is how the two drift.

### 10.1 `Makefile`

| Target | Action |
|---|---|
| `make` / `make dev` | `cmake --preset linux-debug && cmake --build build/linux-debug -j` |
| `make release` | `cmake --preset linux-release && cmake --build build/linux-release -j` |
| `make test` | build `tests`, run it, then `ctest` |
| `make smoke` | build + run 50-iteration reload test |
| `make run` | launch `build/linux-release/RayWaves` |
| `make dist` | `Distribution/distribute.sh -BuildConfig Release -OutputDir dist` |
| `make clean` / `make distclean` | remove build tree / also remove `CMakeCache.txt` |
| `make format` | `Tools/run_analysis.sh` (clang-tidy removed entirely, `make tidy` gone) |
| `make help` | list targets |

`build` + `dist` already in `.gitignore`.

### 10.2 `Distribution/distribute.sh`

Ports `distribute.ps1:1-180` line for line. Structure carries unchanged:

1. Verify `build/linux-release` outputs exist, else configure + build (`distribute.ps1:18-32`)
2. Kill stale processes matching dist or build path (`distribute.ps1:63-65`)
3. Create `Core/`, `Core/Engine/`, `Core/raylib/{include,lib,bin}/`, `Documentation/` (`:50-57`)
4. Copy runtime to `Core/runtime` — **keep name**, `Editor/ExportService.cpp:146` probes it (`:68`)
5. Copy editor binary if present (`:71-72`)
6. Stage `libEngine.a` to `Core/` (`:78`)
7. Stage `libraylib.so` to `Core/raylib/bin/` + dist root so runtime resolves it (`:81-86`)
8. Copy `EngineContent/`, `Engine/*.{h,cpp}` (`:90`, `:95-96`)
9. Install `dist_CMakeLists.txt` as `Core/CMakeLists.txt` (`:99`)
10. Copy three documentation guides (`:102-104`)
11. Copy templates, strip build artifacts + `.raywaves/` (`:107-123`)
12. Copy `config.ini` (`:158`)

**Skip `distribute.ps1:127-155` entirely** — Zig, rcedit, Ninja, CMake bundling. Linux plugin builds with system toolchain; nothing to vendor.

### 10.3 `Tools/run_analysis.sh` (now format-only: clang-tidy removed)

Ports `run_analysis.bat`. Probe `PATH` first, then `/usr/lib/llvm-*/bin`. Keep `tidy`, `tidy-perf`, `tidy-fix`, `format`, `format-check`, `build-tidy`, `report`, `all` subcommands from `run_analysis.bat:8-17`.

---

## 11. Export feature: tarball + `install.sh`

Chosen over AppImage + Flatpak: keeps zero-install promise, no new toolchain, no runtime FUSE dependency, no network fetch at build. AppImage revisit later if zero-install proves insufficient. Flatpak conflicts outright — engine's in-app compiler (`Editor/GameEditor.cpp:880-980`) writes into arbitrary user project dirs.

### 11.1 Export flow

`Editor/ExportService.cpp::RunExport` becomes:

1. Build `GameLogic.so` via system `cmake --build` (Phase D already removed `powershell`, `cd /d`, `_popen` paths)
2. Copy runtime, `GameLogic.so`, `libraylib.so`, `Assets/`, `config.ini` into export dir
3. Generate `run.sh` — sets `LD_LIBRARY_PATH="$ORIGIN"`, execs runtime
4. Generate `install.sh` — installs `.desktop`, installs icon PNG into `~/.local/share/icons/hicolor/256x256/apps/`, runs `update-desktop-database`
5. Validate (§11.2)
6. Tar the dir

`install.sh` icon block = byte-for-byte same work as Phase F's `RegisterRayWavesFileAssociation`. **Factor into one shared helper** in `Engine/Platform/PlatformDesktop.h` so editor menu item + export script cannot drift.

### 11.2 Validation changes

`b_ValidateExportFolder` at `Editor/ExportService.cpp:63-111`:

- `:83` requires `extension() == ".exe"`. Linux binaries carry **no extension**. Validate by filename or ELF magic (`\x7fELF`).
- `:97` requires `GameLogic.dll` → `GameLogic.so`
- `:98` requires `libraylib.dll` → `libraylib.so`

Rest of file: `:147-151` hardcodes `Core/runtime.exe`, `build/zig-release/game.exe`, `libraylib.dll` — use `PlatformPaths` constants; `:204`, `:207`, `:227` `.dll` literals; `:213` `settings.m_GameName + ".exe"` — drop suffix; `:230` `libraylib.dll`; `:274-288` rcedit — removed, icon lives in `.desktop` `Icon=` key; `:176-179` CMake auto-fetch — deleted.

### 11.3 Pre-existing bug worth fixing in passing

`Editor/ExportService.cpp:263-270` resolves default icon to `Core/EngineContent/raylib.ico`, falls back to `EngineContent/raylib.ico`. **Neither exists** — `EngineContent/` holds `icon.ico` + `icon.png`. Branch always falls through to warning at `:289-292`. Broken on Windows too, not a porting regression. Use `EngineContent/icon.png`.

---

## 12. Verification

```sh
make dev
make test
make smoke
```

Confirm no Windows-specific residue in first-party code — both must return nothing:

```sh
rg -n --glob '!Editor/imgui/**' --glob '!Editor/tinyfiledialogs/**' \
      --glob '!Tests/doctest/**' --glob '!Engine/raygui.h' --glob '!Editor/rlImGui/extras/**' \
      -e 'windows\.h|Windows\.h|_WIN32|__declspec|LoadLibrary|GetProcAddress|FreeLibrary' .

rg -n -e '\.dll|\.exe|libraylib|cmd\.exe|cd /d|_popen|_pclose|\.bat|\.ps1|powershell' \
      --glob '!Documentation/**' --glob '!Editor/imgui/**' \
      --glob '!Editor/tinyfiledialogs/**' --glob '!Tests/doctest/**' .
```

### Build and link

- [x] Five targets build: `RayWaves`, `game`, `GameLogic`, `tests`, `smoketest`
- [x] `GameLogic.so` produced + `dlopen`-able
- [x] `ldd GameLogic.so` resolves `libraylib.so`, no "not found"
- [x] `ldd RayWaves` resolves `libraylib.so`

### Hot reload — the feature that matters most

- [ ] Editor hot-reload works: recompile `GameLogic`, running editor reloads, no restart
- [x] 50-iteration smoke test passes — real proof shadow-copy survived
- [x] `CleanupStaleShadowCopies` removes `.shadow.*.so`. **Verify by hand** — `.dll`-only filter at `Game/DllLoader.cpp:88` = silent failure, not build error
- [x] ABI mismatch path still reports correctly (`GameLogicLoader.cpp:180-194`)

### Editor and shell

- [x] Exported folder validates, game runs standalone from `run.sh`
- [x] `install.sh` installs working `.desktop` entry + icon
- [ ] `.raywaves` double-click opens project in editor
- [ ] Editor terminal runs command in project dir (`cd`, not `cd /d`)
- [ ] Editor preferences + recent projects resolve to same dir and **persist across launches** — `getenv("APPDATA")` falling to CWD = silent bug, not error

### Memory safety

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DRAYWAVES_SANITIZERS=ON
cmake --build build/asan -j"$(nproc)"
```

- [x] Editor clean under ASan — 8s headless run, no ASan/UBSan findings (`ASAN_OPTIONS=log_path=/tmp/rwasan`). LSan exit check needs GUI close, not reachable headless; tests link editor sources and are LSan-clean.
- [x] Smoke test clean under ASan — 50 iterations, exit 0, no leaks.
- [ ] No use-after-free from detached build thread at `Editor/ProcessRunner.cpp:191` — callbacks capture editor at `Editor/GameEditor.cpp:954-978`, `m_bThreadCancelFlag` checked only inside callback body, so destroying editor mid-build races. Not Windows-specific; Linux builds slower, hits it more.

`CMakeLists.txt:33-40`: `RAYWAVES_SANITIZERS` option gates `-fsanitize=address,undefined`; the `else()` branch keeps `-fno-sanitize=all` for normal builds (the old hardcoded flag that defeated this).

---

## Appendix A — Files requiring edits

| File | Phase | Change |
|---|---|---|
| `Editor/GameEditor.cpp` | B | Fix `:895` `GetApplicationDirectory` (blocking); `:923` `cd /d`; `:909`, `:935` `cmake.exe`; `:913-916`, `:939-942` powershell; `:902-905` reject before spawn |
| `CMakeLists.txt` | A, B | 11 `if(WIN32)` guards; delete `:4-16`, `:21-31`; `.so` staging; rpath; sanitizer option |
| `Game/DllLoader.cpp` | C | `<dlfcn.h>`; 3 loader calls; `/proc/self/exe`; pid/tick; `.dll` filter; `fs::absolute`; drop retry loop |
| `Game/DllLoader.h` | C | Remove `:15` `WIN32_LEAN_AND_MEAN` |
| `Game/PeCrtCheck.cpp`, `.h` | A | Delete, plus `CMakeLists.txt:112`, `:254`, `:292`, `:329` |
| `Game/main.cpp` | C | Remove `:4` `crtdbg.h` + `:36-42` `_Crt*` |
| `Game/game.cpp` | C | `:93` `.dll` suffix |
| `Engine/ProjectManager.cpp` | A, C, E | Remove `:6-7`; `/proc/self/exe` at `:13-14`, `:323-324`; `:17`, `:25` sentinel; `:100` XDG; `:320`+ `\` rewrites; regenerate `:344-401` CMake text; `CONFIGURE_DEPENDS` at `:390` |
| `Engine/WindowUtils.cpp` | E | Remove `:8-9`, `:14` headers + `:15-16` pragmas; stub `:20-34`; delete `:36-53`; return `true` at `:55-66` |
| `Engine/Project.cpp` | C | `:26` default entry DLL suffix |
| `Engine/GameState.h` | A | Restate `:18-22` CRT note |
| `Editor/FileAssociation.cpp` | F | Remove `:2-3`; `/proc/self/exe` at `:9-10`; XDG rewrite of `:19-58` + `:63-85` |
| `Editor/EditorUtils.cpp` | E | Remove `:3-4`; `xdg-open` at `:14`, `:29`; drop `wstring` at `:28`; add `\` at `:35` |
| `Editor/ProcessRunner.cpp` | D | Remove `:9-10`; `popen` branch; `:95` `/bin/sh -c` |
| `Editor/terminal/terminal.cpp` | D | Remove `:12-14`; `:691` `cd`; `:694`, `:713`, `:729` popen; `:701` `strerror_r` |
| `Editor/ExportService.cpp` | D, H | `:83`, `:97-98`, `:147-151`, `:176-179`, `:183`, `:185`, `:197`, `:204`, `:207`, `:213`, `:227`, `:230`, `:263-270`, `:274-288` |
| `Editor/Panels/ExportPanel.cpp` | F | `:82` icon filter; generated desktop entry at `:419-435` unaffected |
| `Editor/Panels/MainMenuBar.cpp` | F | No code change; verify checkmark semantics |
| `Tests/DllLoader_t.cpp` | I | `:55` zig → g++; `:79` `.so` + rpath; `:34-36`, `:47-49` `__declspec` |
| `Tests/ExportService_t.cpp` | I | `:104-116` fixture suffixes |
| `Tests/GameEditor_t.cpp` | I | `:33-45` rewrite |
| `Tests/SmokeTest.cpp` | I | `:32` `.so` |
| `CMakePresets.json` | B | Add `linux-base`, `linux-debug`, `linux-release` |
| `Distribution/dist_CMakeLists.txt` | B | `:81` copy `libraylib.so` |
| `EngineContent/app.rc`, `EngineContent/icon.ico` | B | Deleted |
| `Tools/run_analysis.bat` | G | Replaced by `.sh`, then trimmed to format-only — clang-tidy removed |
| `Distribution/distribute.ps1` | G | Replace with `.sh` |
| `Distribution/create_distribution.bat` | G | Deleted; `make dist` covers |
| `Tools/zig-cc.bat`, `zig-cxx.bat`, `setup_zig.ps1` | A | Delete |
| `Tests/run_tests.bat`, `run_all.bat`, `run_smoketest.bat` | A | Delete |
| `.clangd` | B | Repoint `CompilationDatabase` |
| `.gitignore` | B | Add `Distribution/Templates/**/*.so` |
| **New:** `Makefile`, `Distribution/distribute.sh`, `Tools/run_analysis.sh` | G | §10. `.clang-tidy` deleted, `ENABLE_CLANG_TIDY` CMake option deleted, `make tidy` deleted |
| **New:** `Engine/Platform/PlatformModule.h`, `PlatformPaths.h`, `PlatformDesktop.h` | C, E, F | §8. `PlatformProcess`/`PlatformShell` seams not created: `popen` lives directly in `Editor/ProcessRunner.cpp`, `xdg-open` spawn directly in `Editor/EditorUtils.cpp`. |
| `Distribution/Templates/*/GameLogic/RootManager.cpp` (×3) | C | Delete `__declspec(dllexport)` |
| `Distribution/Templates/*/project.raywaves` (×3) | C | `entryDll=GameLogic.so` |

## Appendix B — Win32 to POSIX/XDG mapping

| Win32 | Linux | Sites |
|---|---|---|
| `LoadLibraryA` | `dlopen(path, RTLD_NOW\|RTLD_LOCAL)` | `Game/DllLoader.cpp:140`, `:201`, `:211`, `:219` |
| `GetProcAddress` | `dlsym` | `Game/DllLoader.cpp:272` |
| `FreeLibrary` | `dlclose` | `Game/DllLoader.cpp:230` |
| `HMODULE` | `void*` | `Game/DllLoader.cpp`; `Editor/ProcessRunner.cpp:18-54` |
| `GetModuleFileNameA` | read `/proc/self/exe` | `Game/DllLoader.cpp:39`; `Engine/ProjectManager.cpp:14`, `:324`; `Engine/WindowUtils.cpp:46`; `Editor/FileAssociation.cpp:10` |
| `GetCurrentProcessId` | `getpid()` | `Game/DllLoader.cpp:186` |
| `GetTickCount64` | `std::chrono::steady_clock::now()` | `Game/DllLoader.cpp:187` |
| `MAX_PATH` | `PATH_MAX` (4096) or `std::filesystem::path` | 5 files, §7 |
| `CreateProcessA` | `popen("...","r")` / `fork`+`execvp` | `Editor/ProcessRunner.cpp:100` |
| `CreatePipe` / `ReadFile` | `pipe()` / `read()` | `Editor/ProcessRunner.cpp:74`, `:137` |
| `WaitForSingleObject` / `GetExitCodeProcess` | `waitpid()` | `Editor/ProcessRunner.cpp:181-184` |
| `cmd.exe /C` | `/bin/sh -c` | `Editor/ProcessRunner.cpp:95` |
| `_popen` / `_pclose` | `popen` / `pclose` | `Editor/ExportService.cpp:185`, `:197`; `Editor/terminal/terminal.cpp:694`, `:713`, `:729` |
| `strerror_s` | `strerror_r` | `Editor/terminal/terminal.cpp:701` |
| `ShellExecuteW(explore)` | `xdg-open` / `gio open` | `Editor/EditorUtils.cpp:14` |
| `ShellExecuteW(open, url)` | `xdg-open` | `Editor/EditorUtils.cpp:29` |
| `SHGetFolderPathA(CSIDL_APPDATA)` | `$XDG_CONFIG_HOME` / `$HOME/.config` | `Engine/ProjectManager.cpp:100` |
| `getenv("APPDATA")` | `getenv("XDG_CONFIG_HOME")` | `Editor/EditorPreferences.cpp:14` |
| `RegCreateKeyExA` / `RegSetValueExA` | `~/.local/share/mime/packages/*.xml` + `update-mime-database` | `Editor/FileAssociation.cpp:22-56` |
| `SHChangeNotify` | `update-desktop-database` | `Editor/FileAssociation.cpp:58` |
| `DwmSetWindowAttribute` | none | `Engine/WindowUtils.cpp:30`, `:33` |
| `ExtractIconA` + `WM_SETICON` | raylib `SetWindowIcon` (already used) | `Engine/WindowUtils.cpp:47-51` |
| `crtdbg.h` / `_CrtSetDbgFlag` | `-fsanitize=address` | `Game/main.cpp:4`, `:36-42` |
| `__declspec(dllexport)` | delete (GCC exports `extern "C"` by default) | 10 sites, §4.5 |
| `.rc` + `rcedit` | `.desktop` `Icon=` | `CMakeLists.txt:205`, `:214`, `:255`, `:263`; `Editor/ExportService.cpp:274` |
| `-Wl,--subsystem,windows` | none | `CMakeLists.txt:208`, `:257` |
| `cd /d` | `cd` | `Editor/GameEditor.cpp:923`; `Editor/ExportService.cpp:183`; `Editor/terminal/terminal.cpp:691` |
| `localtime_s` | `localtime_r` | already handled, `Editor/terminal/terminal.cpp:218` |

## Appendix C — Hardcoded Windows literals

**`.dll`** — `Engine/Project.cpp:26`; `Game/game.cpp:93`; `Game/DllLoader.cpp:88`; `Tests/SmokeTest.cpp:32`; `Editor/ExportService.cpp:97`, `:204`, `:207`, `:227`; `Tests/ExportService_t.cpp:107`; three `project.raywaves` line 7.

**`.exe`** — `Editor/ExportService.cpp:83`, `:147`, `:150`, `:213`; `Editor/Panels/ExportPanel.cpp:65`; `Tests/ExportService_t.cpp:104`; `Distribution/distribute.ps1:35`, `:39`, `:68`, `:71-72`, `:165`.

**`libraylib.dll`** — `Editor/ExportService.cpp:98`, `:148`, `:151`, `:160`, `:230`; `Tests/ExportService_t.cpp:110`, `:116`; `Distribution/distribute.ps1:37`, `:40`, `:81`, `:86`, `:168`; `Distribution/dist_CMakeLists.txt:81`.

**`libraylib.dll.a`** — `CMakeLists.txt:100-103`; `Distribution/distribute.ps1:82`.

**`zig.exe`** — `CMakeLists.txt:5`; `Engine/ProjectManager.cpp:344`; `Tools/setup_zig.ps1:14`; `Tests/DllLoader_t.cpp:55`.

**`ninja.exe`** — `Engine/ProjectManager.cpp:360`, `:372`; `Tools/setup_zig.ps1:62`; `Distribution/distribute.ps1:145`.

**`cmake.exe`** — `Editor/GameEditor.cpp:909`, `:935`; `Editor/ExportService.cpp:172`; `Tests/GameEditor_t.cpp:33`, `:40`.

**`rcedit.exe`** — `CMakeLists.txt:21`, `:214`, `:263`; `Editor/ExportService.cpp:274`; `Distribution/distribute.ps1:139`; `Tools/setup_zig.ps1:50`.

**`setup_zig.ps1`** — `Engine/ProjectManager.cpp:17`, `:25`; `Editor/GameEditor.cpp:914`, `:940`; `Editor/ExportService.cpp:177`; `CMakeLists.txt:8`, `:24`; `Distribution/distribute.ps1:136`.

**`build_gamelogic.bat`** — `Editor/GameEditor.cpp:929`; `Distribution/README.md:9`.

**`raylib.ico`** — `Editor/ExportService.cpp:267-268`. **Does not exist.** Use `icon.png`.

**`icon.ico`** — `CMakeLists.txt:214`, `:263`; `EngineContent/app.rc:1`.

**Build layout `build/zig-release`, `build/zig-debug`** — `Editor/ExportService.cpp:147-148`; `Distribution/distribute.ps1:18`, `:22`, `:24`; `Distribution/create_distribution.bat:5`, `:7`, `:14`; `Tests/run_tests.bat:6`, `:10`, `:14`; `Tests/run_all.bat:6`, `:10`, `:14`, `:18`, `:22`; `Tests/run_smoketest.bat:8`, `:15`, `:24`; `Tools/run_analysis.bat:32`, `:74`.

**`C:\Program Files\LLVM\bin`** — `CMakeLists.txt:44`; `Tools/run_analysis.bat:31`.

## Appendix D — Vendored, already cross-platform

No work. Verified guards present.

| Library | Location | Evidence |
|---|---|---|
| Dear ImGui core | `Editor/imgui/` | Upstream tree; `imconfig.h:41-46` documents Win32 options, all off by default |
| ImGui OpenGL3 loader | `Editor/imgui/backends/imgui_impl_opengl3_loader.h` | Both paths: `#if defined(_WIN32)` at `:628-653` (`LoadLibraryA("opengl32.dll")`) + POSIX `dlopen`/`glXGetProcAddressARB`/`eglGetProcAddress` at `:766-784` |
| rlImGui | `Editor/rlImGui/rlImGui.h:38-45` | `#if defined(_WIN32)` → `__declspec`, else `__attribute__` |
| tinyfiledialogs | `Editor/tinyfiledialogs/` | `#ifdef _WIN32` at `:72`, `:190`, `:251`; Linux shells out `zenity`/`kdialog` |
| raygui | `Engine/raygui.h`, `Engine/raygui_impl.cpp` | Pure raylib API |
| doctest | `Tests/doctest/doctest.h` | Header-only |
| raylib 6.0 | Fetched, `CMakeLists.txt:75-79` | Upstream. Linux needs X11 or Wayland dev headers. **Verify `6.0` tag resolves** + no used API removed in it |

## Appendix E — Risks carried into the port

Not Windows-specific. Hit more often once builds run through `sh -c` and on slower FS.

| Site | Risk |
|---|---|
| `Editor/ProcessRunner.cpp:191` | Worker thread `detach()`ed, callbacks capture editor (`Editor/GameEditor.cpp:954-978`). `m_ThreadCancelFlag` checked only inside callback body — destroying editor mid-build races. Convert to `std::jthread` + cancel token |
| `Editor/GameEditor.cpp:902-905` | Unsafe project path becomes `build_cmd = "echo ERROR: ..."`, still spawned at `:951`. Rejection surfaces as failed build. Reject before spawning |
| `Editor/ExportService.cpp:197` | `_pclose` returns raw wait status — signal deaths + nonzero exits indistinguishable. `pclose` returns exit status |
| `Game/PeCrtCheck.cpp:43-46` | `reinterpret_cast<const uint16_t*>` on file-derived offsets unaligned — fine on x86, UB + SIGBUS on aarch64. Moot once deleted |
| `Editor/GameEditor.cpp:931`, `:944` | `""…""` wrapping = `cmd.exe` quote-stripping workaround (documented at `Editor/ExportService.cpp:278`). Wrong under `sh`, not redundant. Re-derive, do not copy |
