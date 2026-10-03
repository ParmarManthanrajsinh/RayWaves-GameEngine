# Distribution Scripts

Packaging scripts for creating RayWaves distribution packages.

## Files

- `distribute.sh` — builds `linux-release` and assembles the `dist/` package
  (run via `make dist` or directly with `-BuildConfig Release -OutputDir dist`)
- `dist_CMakeLists.txt` — CMake config for the distributed dev environment
  (installed as `Core/CMakeLists.txt`, builds each project's `GameLogic.so`)
- `config.ini` — Default game config template
- `Templates/` — Project templates shipped with the distribution
