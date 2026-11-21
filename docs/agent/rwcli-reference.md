# rwcli reference

`rwcli` is the agent CLI shipped with RayWaves. It lives in the package
(`rwcli` / `rwcli.exe` next to the editor), not inside each project.

## Locate it

From a project root:

```powershell
# Windows
.\.raywaves\bin\rwcli.cmd help
```

```sh
# Linux
.raywaves/bin/rwcli help
```

The wrapper is generated on project open/create and points at the installed
package. If it is missing or stale, read `.raywaves\tooling.ini`:

```ini
[rwcli]
available=1
path=/opt/RayWaves/rwcli        # Linux
# path=D:\RayWaves\rwcli.exe  # Windows
packageRoot=/opt/RayWaves
engineRoot=/opt/RayWaves
version=0.1.0

[project]
root=/home/me/Games/MyGame
sourceDir=GameLogic
assetDir=Assets
entryDll=GameLogic
```

`available=0` means the package CLI was not found when the project was opened.
Build it (`cmake --build build/zig-release --target rwcli`), reopen the
project, or install `rwcli` on `PATH`; the wrapper falls back to `where` on
Windows and `command -v` on Linux.

Regenerate by opening the project in RayWaves or running `rwcli lsp` from the
package. Do not guess the package location.

In the development repo:

```powershell
# Windows
.\build\zig-release\rwcli.exe help
```

```sh
# Linux
./build/zig-release/rwcli help
```

The CLI is not added to the global `PATH`; use the project wrapper or the full path.
VS Code's integrated terminal gets the package path from
`.vscode/settings.json`.

## Commands

Examples abbreviate the wrapper as `rwcli`. On Windows use
`.\.raywaves\bin\rwcli.cmd`; on Linux use `.raywaves/bin/rwcli`.

| Command | Purpose |
|---|---|
| `help [command]` | general or per-command usage |
| `--version` | print the CLI/engine version |
| `doctor <project>` | toolchain, manifest, scenes, lock, IDE config |
| `info <project>` | manifest paths, scenes, tools |
| `maps <project>` | map ids from the DLL, or scene stems |
| `validate <project>` | manifest, scene ids, textures, `RW_GAME` |
| `build <project>` | configure + build GameLogic |
| `run <project>` | headless fixed-frame run |
| `lsp <project>` | editor configs + compile database |
| `scene <sub> <project>` | list/add/set/remove/duplicate scene objects |
| `assets <sub> <project>` | list/import assets |

### help

```text
rwcli help
rwcli help run
rwcli build --help
rwcli --version
```

### doctor

```text
rwcli doctor . --json
rwcli doctor . --fix --json
```

Exit `3` when a check is `error`. `--fix` clears a stale build lock.

### info

```text
rwcli info . --json
```

### build

```text
rwcli build . --json
rwcli build . --clean --jobs 8 --json
```

- Output: `.raywaves/build-agent/`.
- Lock: `.raywaves/agent.lock`; exit `7` when another live build owns it.
- `--timeout` exits `6` with `build_timeout`.
- `diagnostics[]`: `file`, `line`, `column`, `severity`, `message`.
- `rawTail`: last build lines.

### run

```text
rwcli run . --frames 120 --json
rwcli run . --frames 120 --screenshot shot.png --state state.json --json
rwcli run . --frames 120 --input input.txt --json
```

Flags: `--frames`, `--dt`, `--seed`, `--screenshot`, `--screenshot-frame`,
`--state`, `--size WxH`, `--timeout`, `--visible`, `--audio`, `--input`.
Paths are resolved relative to the project root.

Input script:

```text
0,KEY_RIGHT,down
30,KEY_RIGHT,up
0,mouse,640,360
```

Result includes `map`, `entities[]`, `state`, `screenshot {path,bytes,nonEmpty}`.
A blank screenshot reports `nonEmpty:false`.

```json
{"ok":true,"map":"Simple2D","frames":120,"dt":0.0166667,
 "durationMs":900,"seed":1,"input":null,
 "screenshot":{"path":"...","bytes":38000,"nonEmpty":true},
 "entities":[{"id":1,"name":"Player","type":"Player",
              "position":[400,305],"size":[48,64],"rotation":0,"visible":true}],
 "state":{"rw.entity.count":1}}
```

`run` prints the result JSON even without `--json`; the flag is accepted for
consistency with the other commands.

The hidden window still needs an OpenGL context. On a machine without a GPU
driver, pass `--visible` or run on a desktop session; `run_window` reports the
failure. On headless Linux use `xvfb-run -a` and optionally
`LIBGL_ALWAYS_SOFTWARE=1`.

### lsp

```text
rwcli lsp . --json
rwcli lsp . --check --json
rwcli lsp . --force --json
```

Writes `.clangd`, `.vscode/`, `AGENTS.md`, `.agents/skills/raywaves-game/`
(SKILL.md + reference.md), `.raywaves/tooling.ini`, `.raywaves/bin/rwcli.cmd`,
`.raywaves/bin/rwcli`, and `compile_commands.json`. `--check` runs clangd over
`GameLogic/**/*.cpp` and returns `diagnostics[]`. Use `--force` after moving the
project or updating the engine; it refreshes IDE paths and tooling but does not
overwrite `AGENTS.md`, `SKILL.md`, or `reference.md`.

### scene

```text
rwcli scene list . --json
rwcli scene add . --map Platformer2D --name Crate --texture crate.png --position 620,300
rwcli scene set . --map Platformer2D --id 1 --position 700,300
rwcli scene duplicate . --map Platformer2D --id 1
rwcli scene remove . --map Platformer2D --id 1
```

`--map` is required when more than one `.scene` exists. `add` accepts
`--texture`, `--position X,Y`, `--scale S|SX,SY`, `--rotation`, `--layer`.
`set` accepts `--name`, `--texture`, `--position`, `--rotation`, `--layer`,
`--visible 0|1`. Use `--dry-run` to validate without writing.

### assets

```text
rwcli assets list . --json
rwcli assets import . <file>
```

## Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 2 | usage |
| 3 | project/validation/LSP |
| 4 | build |
| 5 | run |
| 6 | timeout |
| 7 | locked |
| 8 | crash |

## Shell notes

Windows (PowerShell):

- Use `.\` for local executables.
- Quote paths with spaces.
- Engine logs are printed to stderr. To capture JSON cleanly:
  ```powershell
  $json = & .\.raywaves\bin\rwcli.cmd info . --json 2>$null | ConvertFrom-Json
  ```
- If `$ErrorActionPreference = "Stop"`, set it to `Continue` around native
  calls or redirect stderr.

Linux (bash):

- Use `./` for local executables.
- Quote paths with spaces.
- Engine logs are printed to stderr. To capture JSON cleanly:
  ```sh
  json=$(.raywaves/bin/rwcli info . --json 2>/dev/null)
  ```
- Prefer `command -v rwcli` over `which` when probing `PATH`.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `rwcli` is not recognized | use the generated wrapper or the absolute path |
| `invalid_project` | pass the folder containing `project.raywaves` |
| `dll_missing` | run `build` first |
| `build_locked` | wait; `doctor --fix` after the owner died |
| `clangd_missing` | install LLVM (`clangd` or `clang-tools-extra` on Linux) |
| stale LSP paths | `lsp --force` |
| wrapper missing | read `tooling.ini` or open the project once |
| `run_window` on a headless host | run on a desktop session or use `xvfb-run -a` |
