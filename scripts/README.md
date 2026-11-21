# Scripts

Developer entry points. Every task exists in a Windows-native and a Linux-native
form. Both resolve the repository root from their own location, so they can run
from any working directory, and both delegate to the same CMake presets and
targets.

| Task | Windows | Linux | Purpose |
|---|---|---|---|
| Build | `build.ps1` | `build.sh` | Configure and build a preset |
| Test | `test.ps1` | `test.sh` | Build and run unit/integration/smoke suites |
| Bench | `bench.ps1` | `bench.sh` | Build and run `raywaves_benchmarks` |
| Package | `package.ps1` | `package.sh` | Build an engine distribution |
| Agent smoke | `agent-smoke.ps1` | `agent-smoke.sh` | Build + run a template through `rwcli` |
| Check | `check.ps1` | `check.sh` | Format check + docs check + tests |
| Docs check | `check-docs.ps1` | `check-docs.sh` | Stale tokens, fences, relative links |
| Clean | `clean.ps1` | `clean.sh` | Remove a build directory (host switch) |

The CMake presets are identical on both platforms. Their historical names are
kept for compatibility; on Linux they select system GCC/G++, not Zig:

```text
zig-debug
zig-release
```

Examples:

```powershell
scripts/build.ps1 -Preset zig-release -Target main
scripts/test.ps1 -Suite unit
scripts/bench.ps1 --filter Profiler
scripts/package.ps1 -IncludeCompiler -Zip
```

```sh
./scripts/build.sh --preset zig-release --target main
./scripts/test.sh --suite unit
./scripts/bench.sh --filter Profiler
./scripts/package.sh --archive
```

`cmake --workflow --preset ci` runs configure + build + test on either OS.

The Linux scripts assume `bash` and system GCC/G++. `Tools/setup_tools.sh`
fetches Ninja and CMake into `Tools/`; Linux does not use the Zig toolchain.
Windows builds keep the pinned Zig toolchain.
