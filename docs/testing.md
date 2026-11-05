# Testing

RayWaves uses [doctest](https://github.com/doctest/doctest) and CTest.

## Suites

| Suite | Target | Purpose | Typical time |
|---|---|---|---|
| Unit | `raywaves_tests_unit` | Pure engine logic, no DLL builds | < 1 s |
| Integration | `raywaves_tests_integration` | DLL build/load, project files, export helpers | a few seconds |
| Smoke | `raywaves_smoke` | Build GameLogic once, then N load/unload cycles | < 1 s |

Benchmarks are a separate executable; they are not registered with CTest.

## Running

```powershell
# Everything
scripts/test.ps1

# One suite
scripts/test.ps1 -Suite unit
scripts/test.ps1 -Suite integration
scripts/test.ps1 -Suite smoke

# Direct CTest
ctest --preset zig-debug
ctest --test-dir build/zig-debug -L unit --output-on-failure
```

## Adding a test

- Pure engine logic goes in `tests/unit/<Module>_t.cpp`.
- Anything that builds a DLL, touches the filesystem, or runs a process goes in
  `tests/integration/`.
- Put shared helpers in `tests/support/`.
- Add the new file to the matching source list in `tests/CMakeLists.txt`.

Use `TempDir` from `tests/support/TempDir.h` for filesystem tests:

```cpp
#include "support/TempDir.h"

TEST_CASE("My feature writes a file")
{
    TempDir temp_dir("my_feature");
    const auto path = temp_dir.Path() / "output.txt";
    // ...
}
```

Rules:

- Unit tests must not depend on the working directory.
- Integration tests are marked `RUN_SERIAL` in CTest.
- Do not rebuild the engine inside a test loop.
- Keep test output quiet; print only on failure or for benchmarks.
