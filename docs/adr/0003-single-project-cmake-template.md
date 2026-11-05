# 0003 — One project CMake template

## Context

Project builds previously had two recipes: CMake generated in C++ by
`ProjectManager`, and a separate `Distribution/dist_CMakeLists.txt`. They
drifted; the raylib import-library bug existed in both.

## Decision

Keep one template: `packaging/Project.CMakeLists.in`. Dev builds stage it next
to the editor; distributions ship it in `Core/`. `ProjectManager` substitutes
`@TOOLS_DIR@`, `@ENGINE_ROOT@`, `@ENGINE_DIR@`, `@RAYLIB_DIR@`, and
`@PROJECT_NAME@`, then writes `.raywaves/CMakeLists.txt`.

## Consequences

- A project build change is made in one file.
- Dev and packaged builds use the same recipe.
- Template tokens must be kept in sync with `ProjectManager`.
- A missing template fails project open with a clear error.
