# Testing

RayWaves uses [doctest](https://github.com/doctest/doctest) and CTest.

## Suites

| Suite | Target | Purpose | Typical time |
|---|---|---|---|
| Unit | `raywaves_tests_unit` | Pure engine logic, no DLL builds | < 1 s |
| Integration | `raywaves_tests_integration` | GameLogic module build/load, project files, export helpers | a few seconds |
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

# Agent loop smoke test (build + headless run + screenshot)
scripts/agent-smoke.ps1
```

```sh
# Everything
./scripts/test.sh

# One suite
./scripts/test.sh --suite unit
./scripts/test.sh --suite integration
./scripts/test.sh --suite smoke

# Direct CTest
ctest --preset zig-debug

# Agent loop smoke test under a virtual X server
xvfb-run -a ./scripts/agent-smoke.sh
```

The integration suite includes `Cli_t.cpp` and `CliLsp_t.cpp`, which drive
`rwcli` for project info, scene add/list/remove, invalid-project handling,
generated editor/LSP files, and the project-local wrapper
(`rwcli.cmd` on Windows, `rwcli` on Linux).

`scripts/agent-smoke.ps1` / `scripts/agent-smoke.sh` cover doctor → lsp →
build → run → input → validate → lsp `--check`, using the generated wrapper
after the first lsp call.

## Sanitizers

Linux CI runs a separate `sanitize` job (AddressSanitizer +
UndefinedBehaviorSanitizer) in `.github/workflows/ci.yml`. It configures
`build/asan` (Debug, Ninja), builds `raywaves_tests_unit` and
`raywaves_tests_integration`, and runs the `unit` and `integration` CTest
labels with `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

Run the same gate locally:

```sh
chmod +x scripts/check-sanitize.sh   # once per checkout
./scripts/check-sanitize.sh
```

The script reuses `build/asan` when it already exists; delete it with
`rm -rf build/asan` to reconfigure from scratch. Leak detection is disabled
because raylib/X11/GL teardown and forked child processes report allocator
noise that is not ours to fix. Windows builds are not covered: the Zig
toolchain path forces `-fno-sanitize=all` in `cmake/Platform.cmake`.

To instrument the whole build tree instead of just the CI gate — editor,
runtime, `rwcli`, tests, and the fetched raylib — configure with
`RAYWAVES_SANITIZE` (Linux only; a Windows configure fails explicitly):

```sh
cmake --preset zig-debug -DRAYWAVES_SANITIZE=address,undefined
cmake --build build/zig-debug
```

Any comma-separated GCC/Clang sanitizer list works (`address`, `undefined`,
`thread`, `leak`, ...). The flag applies compile and link options to every
target, so backtraces carry `-fno-omit-frame-pointer` and `-g`.

## Valgrind

Valgrind memcheck is the non-instrumented counterpart: run the normal Debug
binaries under it when ASan's shadow memory or a `thread` sanitizer would get
in the way of the bug you are chasing.

```sh
chmod +x scripts/check-valgrind.sh   # once per checkout
./scripts/check-valgrind.sh                    # unit suite (default)
./scripts/check-valgrind.sh --suite all        # every suite, slow
```

The script configures `build/valgrind` (Debug, Ninja, no sanitizers), builds
the requested suite, and runs each binary under
`valgrind --leak-check=full --track-origins=yes`. Definite leaks and any
invalid access fail the run (`--error-exitcode=99`); still-reachable raylib/
X11 teardown noise does not. Child processes such as the `cmake`/`ninja`
subprocesses spawned by the integration and smoke suites stay outside
Valgrind (`--trace-children=no`). Suites run under `xvfb-run -a` when Xvfb is
installed. Install Valgrind with `sudo apt install valgrind`.

Any single binary works the same way:

```sh
valgrind --leak-check=full --error-exitcode=99 \
  ./build/zig-debug/rwcli run <project> --frames 60
```

## Adding a test

- Pure engine logic goes in `tests/unit/<Module>_t.cpp`.
- Anything that builds a shared module, touches the filesystem, or runs a process goes in
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
