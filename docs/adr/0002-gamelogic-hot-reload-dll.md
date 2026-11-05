# 0002 — GameLogic as a hot-reloaded DLL

## Context

Gameplay code must change without restarting the game window. Windows locks
loaded DLL files, so the compiler cannot overwrite a loaded `GameLogic.dll`.

## Decision

Build gameplay as a shared library and load a shadow copy. The original file
stays unlocked for the compiler. The editor/runtime validates the exported
`GetGameLogicAbiVersion` before using the DLL.

## Consequences

- Gameplay state must cross the DLL boundary through `StateBag` plus
  `SaveState` / `LoadState`.
- Objects allocated in the DLL must be destroyed by the DLL.
- ABI mismatches are rejected; rebuild the DLL after engine ABI changes.
- Crashes in gameplay code run in the host process.
