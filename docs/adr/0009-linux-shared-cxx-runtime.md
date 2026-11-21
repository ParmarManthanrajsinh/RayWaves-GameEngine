# 0009 — Linux shared C++ runtime and safe module unload

## Context

The first Linux port kept Zig’s static libc++ runtime. `dlclose` could not
safely detach the plugin’s exit handlers, so old GameLogic mappings were kept
until process exit. That prevented true hot reload and leaked a module mapping
per reload.

Glibc also marks any DSO containing `STB_GNU_UNIQUE` symbols as `NODELETE`.
GCC emits those symbols for inline static/template data by default.

## Decision

- Linux builds use system GCC/G++ and the shared `libstdc++.so.6`.
- Windows keeps the pinned Zig toolchain.
- Linux compiles with `-fno-gnu-unique` so glibc can unload GameLogic modules.
- `DllLoader` uses `dlopen(..., RTLD_NOW | RTLD_LOCAL)` and `dlclose` on unload.
- The smoke suite runs repeated load/unload cycles and checks `/proc/self/maps`
  for leftover shadow modules.

## Consequences

- Hot reload frees the old module mapping instead of leaking it.
- Linux requires GCC/G++ on the machine and in the distribution target.
- `--include-compiler` cannot bundle a Linux compiler.
- Windows behavior is unchanged.
- ABI is bumped to 8 for the editor-entity storage change shipped alongside
  this decision.
