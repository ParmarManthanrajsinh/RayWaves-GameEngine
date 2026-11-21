# Linux

RayWaves builds and runs natively on Linux with the same CMake presets and
targets as Windows.

## Requirements

- GCC 13+ / G++ 13+. Linux uses the system libstdc++ so GameLogic modules can
  be safely unloaded on hot reload.
- CMake 3.25 or newer.
- Ninja (the repo can also fetch it into `Tools/ninja`).
- X11, OpenGL, and ALSA development headers.
- Optional: `clangd` for editor diagnostics, `clang-format` for the format
  check, `zenity` or `kdialog` for native file dialogs.

The Linux package is x86_64 only.

Debian/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libgl1-mesa-dev libasound2-dev zenity clangd
```

Fedora:

```sh
sudo dnf install cmake libX11-devel libXrandr-devel libXinerama-devel \
  libXcursor-devel libXi-devel mesa-libGL-devel alsa-lib-devel \
  zenity clang-tools-extra
```

Arch:

```sh
sudo pacman -S cmake libx11 libxrandr libxinerama libxcursor libxi \
  mesa alsa-lib zenity clang
```

## Build

```sh
cmake --preset zig-release
cmake --build build/zig-release
./build/zig-release/RayWaves
```

Linux uses the system GCC/G++. `cmake --preset` and `cmake --build` are
identical to the Windows commands; Windows keeps using the pinned Zig toolchain.

## Test

```sh
./scripts/test.sh
```

The integration suite builds a small GameLogic module with the host compiler,
loads it through a shadow copy, and exercises `rwcli`. The smoke suite performs
repeated hot-reload cycles.

Run the local gate (format, docs, tests) before a PR:

```sh
./scripts/check.sh
```

Headless machines and CI need an X server. Use Xvfb:

```sh
xvfb-run -a ./scripts/test.sh
xvfb-run -a ./scripts/agent-smoke.sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./scripts/agent-smoke.sh
```

## Run a project

```sh
./build/zig-release/rwcli lsp path/to/Game
./build/zig-release/rwcli build path/to/Game --json
xvfb-run -a ./build/zig-release/rwcli run path/to/Game --frames 60 --screenshot shot.png --json
```

Project wrappers are generated in both forms:

- `.raywaves/bin/rwcli.cmd` — Windows
- `.raywaves/bin/rwcli` — Linux

The wrapper for the current host is regenerated when the project is opened.

## Display

The editor and games use raylib's GLFW backend, which targets X11. On Wayland
desktops, XWayland is used. Native Wayland is out of scope for the first pass.

The editor creates a resizable window and sizes it to roughly 90% of the
current monitor. Tiling compositors should then tile it normally.

### Hyprland

The editor advertises `_NET_WM_WINDOW_TYPE_NORMAL` and gives Hyprland explicit
min/max size hints before the window is mapped. Hyprland floats XWayland
windows whose size hints have no real min/max values, so this keeps the editor
tiled.

If RayWaves still opens floating, add a fallback rule in
`~/.config/hypr/hyprland/rules.lua`:

```lua
hl.window_rule({
    match = { class = "^(RayWaves)$" },
    float = false,
})
```

Then reload:

```sh
hyprctl reload
```

For a large centered floating window instead:

```lua
hl.window_rule({
    match  = { class = "^(RayWaves)$" },
    float  = true,
    size   = "(monitor_w*0.85) (monitor_h*0.85)",
    center = true,
})
```

With a 1.25 monitor scale, XWayland reports logical sizes as
`physical_size / scale`. A 1280x720 request therefore appears as 1024x576
logical. The editor now sizes itself to the monitor, which avoids the
"small window" look.

## File dialogs

The editor uses `tinyfiledialogs`. On Linux it requires `zenity` (GNOME) or
`kdialog` (KDE) for open/save/folder dialogs. Without either, the editor still
runs, but those dialogs fail.

## Packaging

```sh
./scripts/package.sh --archive
# dist/RayWaves-<version>-linux-x86_64.tar.gz
```

The archive preserves executable bits and `$ORIGIN` RPATHs. Extract it and run
`./RayWaves` or `./rwcli` from any location.

## Hot reload

Linux uses `dlopen` with a unique shadow copy for each build, and `dlclose` on
unload. GameLogic and the host share the system libstdc++, and Linux builds use
`-fno-gnu-unique` so glibc does not mark the plugin `NODELETE`. The smoke suite
runs repeated load/unload cycles and verifies no shadow module remains mapped.

`--include-compiler` does not bundle a compiler on Linux; the package uses the
system GCC/G++.
