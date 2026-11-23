# 0011 — Split and extend the `rw` game-facing API

## Context

`Engine/rw.h` had grown into a single header containing input replay, textures,
cameras, entities, levels, and the game exports. Games only needed one include,
but every subsystem change collided in the same file. The facade also stopped at
drawing primitives: gameplay code still hand-rolled collision, audio lifetimes,
sprite timing, timers, and menu hit-testing, or dropped to raw raylib and lost
input replay under `rwcli run --input`.

Generated projects copy only flat `Engine/*.h` files, and `rwcli`/the editor
compile gameplay modules against the same headers. A nested header directory
would not ship without changing the packaging contract.

## Decision

Keep `Engine/rw.h` as the umbrella include, and split the implementation into
flat subsystem headers:

- `rw_input.h` — replay-aware keys/mouse and `rw::ActionMap`.
- `rw_core.h` — `Origin`, `Texture`, `Camera`, `Property`, `Entity`, `Level`,
  `Game`, and the `RW_GAME` exports.
- `rw_audio.h` — move-only `Sound`, `Music`, and `Font` wrappers.
- `rw_collision.h` — `MoveAndCollide` and `b_Overlaps`.
- `rw_animation.h` — `SpriteSheet`, `AnimationClip`, and `Animator`.
- `rw_timer.h` — `Timer`, `Tween`, and shared easing.
- `rw_ui.h` — small immediate-mode widgets driven by `rw` input.

All headers stay header-only and flat. `rw.h` includes every subsystem, so
existing `#include "rw.h"` code continues to compile unchanged.

Every interactive helper reads input through `rw::IsKey*` / `rw::IsMouseButton*`
and `rw::GetMousePosition`, not raw raylib globals. That keeps actions, buttons,
and checkboxes replayable by `rwcli run --input`.

## Consequences

- No ABI bump and no project-format change. Each header is self-contained for
  direct includes, while `rw.h` remains the recommended entry point.
- Packaging continues to copy `Engine/*.h` unchanged; no new include directory
  or generated-project CMake change is required.
- The PCH entry (`rw.h`) now pulls the whole facade, which is intentional: it is
  still the header every gameplay file includes.
- Subsystems are independent enough to change, test, and review separately.
- `Tween` deliberately animates one `float`. Use multiple tweens for `Vector2`
  and colors rather than adding templated variants to the facade.
- Raw raylib input still bypasses replay; the facade cannot fix that at the
  boundary. Docs and templates point at `rw` input instead.
