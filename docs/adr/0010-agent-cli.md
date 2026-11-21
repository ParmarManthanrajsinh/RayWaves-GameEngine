# 0006 — Agent-facing CLI (`rwcli`)

## Context

Gameplay is a DLL built by the editor, and the only way to run it was
`RayWaves.exe` or a windowed `game.exe`. An external agent could edit files but
could not build, observe, or verify the result without a human driving the GUI.
Build errors were only visible in the editor terminal; screenshot verification
was impossible.

## Decision

Add `rwcli`, a console host that operates on an existing project folder:

- `doctor`, `info`, `maps`, `validate`, `build`, `run`, `scene`, `assets`.
- `--json` writes machine-readable results to stdout.
- `run` spawns a child `rwcli __run-worker` process. The worker opens a hidden
  window, steps a fixed number of frames, writes a screenshot and state/entity
  JSON, and exits. The supervisor owns the timeout and reports crashes.
- The worker writes its result to a file, so a gameplay crash still yields a
  JSON error instead of killing the caller.

Shared host code moved into a `RuntimeSupport` static library:
`DllLoader`, `PeCrtCheck`, `ProcessRunner`, `AgentLock`, `GameLogicHost`,
`PlatformInfo`. The editor, runtime, CLI, and tests link the same implementation.

## Contract

- stdout is JSON only when `--json` is passed. Engine and raylib diagnostics go
  to stderr. `Project`, `ProjectManager`, `GameEngine`, `MapManager`, and
  `GameConfig` log through `std::cerr`.
- Exit codes: `0` ok, `2` usage, `3` project/validation, `4` build, `5` run,
  `6` timeout, `7` locked, `8` crash.
- `build` acquires `<project>/.raywaves/agent.lock`. A live owner blocks a
  second build; the editor checks the same lock before compiling. Stale locks
  (dead PID) are reclaimed.
- CLI builds use `.raywaves/build-agent`, so an editor build and an agent build
  do not share a Ninja directory.

## Consequences

- No new project format. Scenes stay INI, manifests stay INI.
- No ABI bump: `t_WindowConfig` gained `b_Hidden`/`b_NoAudio`, `StateBag` gained
  read-only accessors, and `GameLogicHost` is a non-virtual host wrapper.
- Generic state works for any map that registers editor entities. Custom state
  still requires `SaveState`.
- Input replay (`rw::IsKeyDown` and friends + `RAYWAVES_AGENT_INPUT`) is opt-in
  at the game code level; raw raylib calls are not scriptable.
- `AGENTS.md` is generated into each project from
  `packaging/Project.AGENTS.md.in` when missing.
- `rwcli` must ship at the distribution root, next to `RayWaves.exe`, so
  `ProjectManager` resolves the same engine root and raylib DLL.
