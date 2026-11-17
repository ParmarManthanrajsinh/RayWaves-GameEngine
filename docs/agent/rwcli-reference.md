# rwcli reference

`rwcli` is the agent CLI shipped with RayWaves. It lives in the package
(`rwcli.exe` next to `RayWaves.exe`), not inside each project.

## Locate it

From a project root:

```powershell
.\.raywaves\bin\rwcli.cmd help
```

The wrapper is generated on project open/create and points at the installed
package. If it is missing or stale, read `.raywaves\tooling.ini`:

```ini
[rwcli]
available=1
path=D:\RayWaves\rwcli.exe
packageRoot=D:\RayWaves
engineRoot=D:\RayWaves
version=0.1.0

[project]
root=C:\Games\MyGame
sourceDir=GameLogic
assetDir=Assets
entryDll=GameLogic.dll
```

`available=0` means the package CLI was not found when the project was opened.
Build it (`cmake --build build/zig-release --target rwcli`), reopen the
project, or install `rwcli` on `PATH`; the wrapper falls back to `where`.

Regenerate by opening the project in RayWaves or running `rwcli lsp` from the
package. Do not guess the package location.

In the development repo:

```powershell
build\zig-release\rwcli.exe help
```

PowerShell does not run binaries from the current directory by bare name.
Use `.\rwcli.exe` or the full path. `rwcli` is not added to the global `PATH`;
VS Code's integrated terminal gets the package path from
`.vscode/settings.json`.

## Commands

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

```powershell
.\.raywaves\bin\rwcli.cmd help
.\.raywaves\bin\rwcli.cmd help run
.\.raywaves\bin\rwcli.cmd build --help
.\.raywaves\bin\rwcli.cmd --version
```

### doctor

```powershell
.\.raywaves\bin\rwcli.cmd doctor . --json
.\.raywaves\bin\rwcli.cmd doctor . --fix --json
```

Exit `3` when a check is `error`. `--fix` clears a stale build lock.

### info

```powershell
.\.raywaves\bin\rwcli.cmd info . --json
```

### build

```powershell
.\.raywaves\bin\rwcli.cmd build . --json
.\.raywaves\bin\rwcli.cmd build . --clean --jobs 8 --json
```

- Output: `.raywaves/build-agent/`.
- Lock: `.raywaves/agent.lock`; exit `7` when another live build owns it.
- `--timeout` exits `6` with `build_timeout`.
- `diagnostics[]`: `file`, `line`, `column`, `severity`, `message`.
- `rawTail`: last build lines.

### run

```powershell
.\.raywaves\bin\rwcli.cmd run . --frames 120 --json
.\.raywaves\bin\rwcli.cmd run . --frames 120 --screenshot shot.png --state state.json --json
.\.raywaves\bin\rwcli.cmd run . --frames 120 --input input.txt --json
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
failure.

### lsp

```powershell
.\.raywaves\bin\rwcli.cmd lsp . --json
.\.raywaves\bin\rwcli.cmd lsp . --check --json
.\.raywaves\bin\rwcli.cmd lsp . --force --json
```

Writes `.clangd`, `.vscode/`, `AGENTS.md`, `.agents/skills/raywaves-game/`
(SKILL.md + reference.md), `.raywaves/tooling.ini`, `.raywaves/bin/rwcli.cmd`,
and `compile_commands.json`. `--check` runs clangd over
`GameLogic/**/*.cpp` and returns `diagnostics[]`. Use `--force` after moving the
project or updating the engine; it refreshes IDE paths and tooling but does not
overwrite `AGENTS.md`, `SKILL.md`, or `reference.md`.

### scene

```powershell
.\.raywaves\bin\rwcli.cmd scene list . --json
.\.raywaves\bin\rwcli.cmd scene add . --map Platformer2D --name Crate --texture crate.png --position 620,300
.\.raywaves\bin\rwcli.cmd scene set . --map Platformer2D --id 1 --position 700,300
.\.raywaves\bin\rwcli.cmd scene duplicate . --map Platformer2D --id 1
.\.raywaves\bin\rwcli.cmd scene remove . --map Platformer2D --id 1
```

`--map` is required when more than one `.scene` exists. `add` accepts
`--texture`, `--position X,Y`, `--scale S|SX,SY`, `--rotation`, `--layer`.
`set` accepts `--name`, `--texture`, `--position`, `--rotation`, `--layer`,
`--visible 0|1`. Use `--dry-run` to validate without writing.

### assets

```powershell
.\.raywaves\bin\rwcli.cmd assets list . --json
.\.raywaves\bin\rwcli.cmd assets import . "C:\art\player.png"
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

## PowerShell notes

- Use `.\` for local executables.
- Quote paths with spaces.
- Engine logs are printed to stderr. To capture JSON cleanly:
  ```powershell
  $json = & .\.raywaves\bin\rwcli.cmd info . --json 2>$null | ConvertFrom-Json
  ```
- If `$ErrorActionPreference = "Stop"`, set it to `Continue` around native
  calls or redirect stderr.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `rwcli` is not recognized | use the wrapper or `.\rwcli.exe` |
| `invalid_project` | pass the folder containing `project.raywaves` |
| `dll_missing` | run `build` first |
| `build_locked` | wait; `doctor --fix` after the owner died |
| `clangd_missing` | install LLVM |
| stale LSP paths | `lsp --force` |
| wrapper missing | read `tooling.ini` or open the project once |
