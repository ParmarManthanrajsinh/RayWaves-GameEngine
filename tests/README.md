# Tests

doctest + CTest suites.

- `unit/` — fast, pure engine tests.
- `integration/` — DLL builds/loads, filesystem, export helpers.
- `smoke/` — GameLogic build once, then load/unload cycles.
- `support/` — `doctest_main.cpp` and `TempDir.h`.

Run:

```powershell
scripts/test.ps1
ctest --preset zig-debug
```

Add new files to `tests/CMakeLists.txt`. See `docs/testing.md`.
