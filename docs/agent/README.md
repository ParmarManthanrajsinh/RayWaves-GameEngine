# Agent CLI

`rwcli` lets an agent build, inspect, edit, and run an existing RayWaves
project without opening the editor.

`rwcli` lives in the RayWaves package, not in the project. Projects get a
generated wrapper and a `tooling.ini` descriptor. Prefer:

```powershell
.\.raywaves\bin\rwcli.cmd doctor . --json
```

If the wrapper is missing, read `.raywaves\tooling.ini` and use `[rwcli] path`.
Full command reference: [rwcli-reference.md](rwcli-reference.md).

## Commands

From the project root:

```powershell
.\.raywaves\bin\rwcli.cmd doctor . --json
.\.raywaves\bin\rwcli.cmd info . --json
.\.raywaves\bin\rwcli.cmd maps . --json
.\.raywaves\bin\rwcli.cmd validate . --json
.\.raywaves\bin\rwcli.cmd build . --json
.\.raywaves\bin\rwcli.cmd run . --frames 120 --screenshot shot.png --state state.json --json
.\.raywaves\bin\rwcli.cmd run . --frames 120 --input input.txt --json
.\.raywaves\bin\rwcli.cmd scene list . --json
.\.raywaves\bin\rwcli.cmd scene add . --map Platformer2D --name Crate --texture crate.png --position 620,300
.\.raywaves\bin\rwcli.cmd scene set . --map Platformer2D --id 1 --position 700,300
.\.raywaves\bin\rwcli.cmd scene remove . --map Platformer2D --id 1
.\.raywaves\bin\rwcli.cmd assets list . --json
.\.raywaves\bin\rwcli.cmd assets import . <file>
.\.raywaves\bin\rwcli.cmd lsp . --json
.\.raywaves\bin\rwcli.cmd lsp . --check --json
.\.raywaves\bin\rwcli.cmd lsp . --force --json
```

`<project>` is the folder containing `project.raywaves`. The manifest file
itself also works.

## Project bundle

Opening or creating a project writes these when missing:

- `AGENTS.md` from `packaging/Project.AGENTS.md.in`
- `.agents/skills/raywaves-game/SKILL.md` from `packaging/Project.SKILL.md.in`
- `.agents/skills/raywaves-game/reference.md` from
  `packaging/Project.rwcli-reference.md.in`
- `.clangd` and `.vscode/{c_cpp_properties,settings,extensions}.json`
- `.clang-format` copied from the engine

Two tooling files are always refreshed on open/build/lsp because they pin the
current package path:

- `.raywaves/tooling.ini` — `[rwcli] available, path, packageRoot, version`
- `.raywaves/bin/rwcli.cmd` — wrapper used by agents

`.\.raywaves\bin\rwcli.cmd lsp . --force` refreshes the absolute
engine/raylib/Zig paths after moving a project or updating the engine.

## Editor setup

See [editor-setup.md](editor-setup.md) for VS Code/clangd, IntelliSense fallback,
and troubleshooting. Short version: install clangd, open the project, then run
`.\.raywaves\bin\rwcli.cmd lsp . --check --json`.

## Agent loop

1. `doctor` — toolchain, manifest, scenes, lock.
2. Edit `GameLogic/*.cpp` and `Assets/`.
3. `build --json` — fix `diagnostics[]`.
4. `run --frames N --screenshot shot.png --state state.json --json` — inspect
   the screenshot and entity positions.
5. Repeat.

## JSON shape

Success:

```json
{"ok":true,"map":"Platformer2D","frames":120,"durationMs":900,"input":null,
 "screenshot":{"path":"...","bytes":38000,"nonEmpty":true},
 "entities":[{"id":1,"name":"Player","type":"Player",
              "position":[400,305],"size":[64,64],"rotation":0,"visible":true}],
 "state":{"rw.entity.count":1}}
```

Failure:

```json
{"ok":false,"error":{"code":"build_failed","message":"..."},
 "diagnostics":[{"file":"GameLogic/Player.cpp","line":42,"column":7,
                 "severity":"error","message":"..."}],
 "rawTail":"last build lines"}
```

Exit codes: `0` ok, `2` usage, `3` project, `4` build, `5` run, `6` timeout,
`7` locked, `8` crash.

## Run behavior

- Hidden window, fixed `--dt` (default `1/60`), `--frames` steps.
- `--screenshot` is resolved relative to the project root. `nonEmpty` reports
  whether raylib could read the image back.
- `--state` and `--input` paths are also project-relative.
- `--state` writes the dump to a file; the same data is also embedded in the
  run JSON.
- `--visible` shows the window for debugging. `--audio` enables sound.
- `--seed` sets raylib's RNG before the map starts.
- `--input script.txt` replays keys and mouse position deterministically.
- Run in a child process: a gameplay crash returns `run_crashed`, not a dead
  agent shell.
- `run` prints the result JSON even without `--json`.

## Input script

One event per line. Frames are zero-based and match `--dt` steps.

```text
# frame,key,down|up
0,KEY_RIGHT,down
30,KEY_RIGHT,up
0,mouse,640,360
```

Names: `KEY_A`..`KEY_Z`, `KEY_0`..`KEY_9`, arrows, space, enter, escape, F1..F12,
`MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`. Numeric raylib key codes also work.
Gameplay must call `rw::IsKeyDown` / `rw::IsKeyPressed` (not the raw raylib
functions) for replay to apply.

## Limits

- Input replay only applies to projects that call `rw::IsKeyDown`,
  `rw::IsKeyPressed`, `rw::IsMouseButtonDown`, or `rw::GetMousePosition`.
  Raw raylib input still works but is not scriptable.
- LSP configs are generated with absolute engine paths; rerun
  `.\.raywaves\bin\rwcli.cmd lsp . --force` after moving a project or updating the engine.
- `lsp --check` needs `clangd.exe` in `PATH` or `C:/Program Files/LLVM/bin`.
- `--state` is empty for maps that do not override `SaveState` and do not
  register editor entities.
- `run` needs an OpenGL context even with the hidden window. Headless machines
  without a GPU driver must run on a desktop session.
- The editor and CLI must not build at the same time. The agent lock enforces
  this for `rwcli`; the editor reports a locked project instead of building.
  The editor also skips scene saves while the lock is held.
