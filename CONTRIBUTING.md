# Contributing

## Documentation Checklist

When making code changes, update docs too:

1. **Changed folder structure?** Update `Documentation/ARCHITECTURE.md`.
2. **Renamed an exe or file?** `grep -rn "oldname" Documentation/ README.md` before committing — update every hit.
3. **Removed a UI feature?** `grep -rn -i "featurename" Documentation/ README.md` and remove stale references.
4. **Changed build steps or toolchain?** Update *First-Time Compile* section in `DEVELOPER_GUIDE.md` and any related troubleshooting entries.

## Naming Convention

Follow the existing prefixes — do not introduce new styles:

| Prefix | Meaning | Example |
|--------|---------|---------|
| `b_` | `bool` variable or bool-returning function | `b_IsPlaying`, `b_LoadGameLogic()` |
| `m_` | Member variable | `m_GameEngine`, `m_FrameTimes` |
| `s_` | File-local static / static function | `s_LayoutPath`, `s_fDrawSpinner()` |
| `t_` | Type / struct name | `t_WindowConfig`, `t_MapInfo` |
| `F` | Editor struct type | `FThemePreset`, `FBuildMessage` |
| `E` | Enum type | `EBuildStatus`, `EEditorFont` |
| `c_` / `k_` | Compile-time constant | `c_NUM_SEGMENTS`, `k_FrameCount` |
| `m_t` | Member struct instance | `m_SceneSettings`, `m_ExportState` |

## Where New Code Goes

- **Gameplay (player, enemies, levels):** your project's `GameLogic/` DLL — never `Engine/`. Register maps with `RegisterMap<T>()` in `RootManager.cpp`.
- **Editor UI:** a new file pair under `Editor/Panels/` implementing `IEditorPanel`, plus one `b_RegisterPanel<T>()` line in its own `.cpp` (see `Editor/PanelRegistry.h`) — no `GameEditor.cpp` edit needed.
- **Engine systems:** `Engine/` + explicit entry in `CMakeLists.txt` (no `file(GLOB)`) + a `Tests/*_t.cpp` case.
- **Headers:** include only what the header itself uses (`<string>`, `<string_view>`); put `<iostream>`, `<fstream>`, `<filesystem>` in the `.cpp`.
