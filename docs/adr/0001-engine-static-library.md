# 0001 — Engine as a static library

## Context

`Engine/` is shared by the editor, the runtime, and every project's GameLogic
DLL. Shipping a precompiled engine library to project builds caused toolchain
ABI mismatches when the DLL and engine were built by different Zig versions.

## Decision

Build `Engine` as a static library (`libEngine.a` in Zig builds) for host
targets. Project GameLogic builds compile the `Engine/*.cpp` sources directly
through the generated project CMake, so both sides use one compiler invocation
and one runtime.

## Consequences

- Host builds link `Engine` normally.
- Project builds do not link a prebuilt `libEngine.a`.
- Toolchain changes require rebuilding the editor and the project DLL.
- Engine source must stay portable and free of editor-only dependencies.
