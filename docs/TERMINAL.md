# Terminal

The editor terminal has two kinds of tabs:

- **Console** — non-interactive output sink for `std::cout`, `std::cerr`, raylib logs, loader diagnostics, and build output.
- **Shell** — a real shell process attached to a pseudo terminal (ConPTY on Windows, `posix_openpt` on Linux).

## Architecture

```text
term::Terminal
 ├─ Console tab: t_VTermScreen
 └─ Shell tab:   t_TerminalSession
                  ├─ t_VTermScreen  (libvterm screen + bounded scrollback)
                  └─ t_PtySession   (ConPTY/posix_openpt process, reader thread)
```

- `Editor/terminal/vterm_screen.*` wraps libvterm. It owns the visible screen, scrollback, palette, cursor state, and title.
- `Editor/platform/Win32/pty_session.cpp` owns the ConPTY process. ConPTY entry points are loaded from `kernel32.dll` at runtime so the Zig/mingw toolchain does not need import libraries.
- `Editor/platform/Posix/pty_session.cpp` owns the Linux PTY: `posix_openpt`, `grantpt`, `unlockpt`, `fork`, `setsid`, `TIOCSCTTY`, and `TIOCSWINSZ`.
- `Editor/terminal/terminal_session.*` joins a PTY to a screen: output bytes go to `vterm_input_write`, vterm replies (DSR/DA) go back to the PTY, and keyboard input is encoded by libvterm.
- `Editor/terminal/terminal_view.*` renders cells with ImGui and handles mouse selection, scrolling, clipboard, and key mapping.
- `Editor/terminal/shell_detect.*` finds `pwsh.exe`, `powershell.exe`, or `cmd.exe` on Windows and `$SHELL`, `/bin/bash`, `/bin/zsh`, `/bin/fish`, or `/bin/sh` on Linux.

The terminal core (`raywaves_terminal_core`) contains libvterm plus the screen/PTY/session sources. It has no ImGui dependency, so unit and integration tests link it directly.

## Threading

- The PTY reader thread (ConPTY or `poll` on the Linux master fd) reads output bytes and writes them into libvterm; libvterm access is serialised by the screen mutex.
- Input, resize, and rendering happen on the editor UI thread.
- Console `add_text` is called from build and loader threads, so the console screen also uses the mutex.
- `std::cout` and `std::cerr` are captured with a line-buffered streambuf. Partial writes are held until a newline so unbuffered `std::cerr` does not produce one entry per insertion.

## Child process details

Windows uses `CreateProcessW` with:

- `EXTENDED_STARTUPINFO_PRESENT`
- `CREATE_UNICODE_ENVIRONMENT`
- `STARTF_USESTDHANDLES` with all three standard handles null

Linux `fork`s, calls `setsid`, opens the slave PTY, makes it the controlling
terminal, `dup2`s it onto stdin/stdout/stderr, and `exec`s the shell.

The null standard handles matter. Without `STARTF_USESTDHANDLES`, a child inherits the host process's standard handles. When the host has redirected stdio (CI, IDE capture), the shell writes to the host console instead of the ConPTY pipe. ConPTY then creates fresh ConIn/ConOut handles against the pseudo console.

The child environment sets:

- `TERM=xterm-256color`
- `COLORTERM=truecolor`
- `TERM_PROGRAM=RayWaves`

## Scrollback

libvterm reports lines pushed off the primary screen through `sb_pushline4`. The wrapper stores them in a bounded deque (5000 lines by default). `sb_popline` restores lines when libvterm reflows or scrolls back.

The visible viewport is `scrollback + screen`. Mouse wheel, `Ctrl+Home`/`Ctrl+End`, and `Shift+PageUp`/`Shift+PageDown` move the viewport. New output only follows the bottom when the viewport is already at the bottom.

Local mouse selection and paste take precedence over application mouse reporting. Full-screen applications still receive keyboard input and wheel events (wheel is translated to arrow keys in the alternate screen), but do not receive raw mouse clicks.

## Fonts and Unicode

The terminal uses the editor's monospace font (`Consolas`). libvterm handles UTF-8 and wide cells. Glyphs missing from Consolas (for example Nerd Font prompt icons) appear as placeholders; add a fallback font to `EngineContent` and merge it into the mono font if those prompts are required.

## Tests

- `tests/unit/Terminal_t.cpp` covers screen writes, cursor position, scrollback, SGR colors, selection text, key encoding, paste, and shell detection.
- `tests/integration/Pty_t.cpp` spawns the default shell through ConPTY on Windows or a POSIX PTY on Linux, waits for the shell-ready marker, and stops the session. It skips when the PTY backend is unavailable.
