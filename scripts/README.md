# Scripts

Developer entry points. All scripts resolve the repository root from their own
location, so they can run from any working directory.

| Script | Purpose |
|---|---|
| `build.ps1` | Configure and build a preset |
| `test.ps1` | Build and run unit/integration/smoke suites |
| `bench.ps1` | Build and run `raywaves_benchmarks` |
| `package.ps1` | Forward to `packaging/package.ps1` |
| `check.ps1` | Format check + tests |

Examples:

```powershell
scripts/build.ps1 -Preset zig-release -Target main
scripts/test.ps1 -Suite unit
scripts/bench.ps1 --filter Profiler
scripts/package.ps1 -IncludeCompiler -Zip
```
