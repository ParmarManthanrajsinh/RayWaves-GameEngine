# Editor

ImGui-based editor built as the `main` target and output as `RayWaves.exe`.

Key files:

- `GameEditor.cpp` — main loop and orchestration. Keep it thin.
- `Panels/` — self-registering panels implementing `IEditorPanel`.
  - `FileSystemPanel` — Levels / Assets / Code browser.
  - `SceneHierarchyPanel` — scene tree, selection, map switcher.
  - `InspectorPanel` — selected object properties.
  - `SceneWindow` — viewport, entity picking/dragging, MMB pan, wheel zoom, `F` frame.
- `EditorAssets` — flat asset listing, import, and relative-path helpers.
- `ExportService` — game export pipeline (UI-free, testable).
- `GameLogicLoader` — DLL shadow loading, ABI checks, hot reload.
- `ProcessRunner` moved to `Runtime/`; link `RuntimeSupport` instead.
- `imgui/`, `rlImGui/`, `tinyfiledialogs/` — vendored dependencies.

Adding a panel: create a `.h/.cpp` pair in `Panels/`, implement `IEditorPanel`,
and self-register with `b_RegisterPanel<T>()` in the panel's `.cpp`.
