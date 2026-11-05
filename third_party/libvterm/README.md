# libvterm (vendored)

Source: https://github.com/neovim/libvterm (branch `nvim`, a fork of
https://www.leonerd.org.uk/code/libvterm), MIT license.

Only the files needed to build the VT parser/screen are vendored:

- `include/vterm.h`, `include/vterm_keycodes.h`
- `src/*.c`, `src/*.h`, `src/*.inc`, `src/encoding/*.inc`

The generated `.inc` tables are committed upstream, so no Perl is required.

Build wiring lives in `cmake/Editor.cmake` under `LIBVTERM_SRC`. Do not format
vendored sources with the project clang-format; keep them close to upstream.
