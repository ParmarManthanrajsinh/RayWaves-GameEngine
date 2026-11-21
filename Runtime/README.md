# Runtime

Host executables and the module loading layer.

- `main.cpp` — editor entry point (`RayWaves` / `RayWaves.exe`).
- `game.cpp` — standalone runtime entry point (`game` / `game.exe`).
- `DllLoader` — shadow-copy load/unload, symbol lookup, ABI diagnostics.
- `platform/Win32/` — `LoadLibrary` loader, process runner, PE CRT scan.
- `platform/Posix/` — `dlopen` loader, process runner, POSIX CRT stub.
- `PeCrtCheck` — PE import-table CRT compatibility check (Windows only).

`DllLoader` is also used by the integration and smoke tests. Keep it free of
editor-only dependencies.
