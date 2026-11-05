# Runtime

Host executables and the DLL loading layer.

- `main.cpp` — `RayWaves.exe` entry point (editor).
- `game.cpp` — `game.exe` entry point (standalone runtime).
- `DllLoader` — shadow-copy load/unload, symbol lookup, ABI diagnostics.
- `PeCrtCheck` — PE import-table CRT compatibility check.

`DllLoader` is also used by the integration and smoke tests. Keep it free of
editor-only dependencies.
