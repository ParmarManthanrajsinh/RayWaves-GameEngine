# Troubleshooting Guide

*Fixing the waves when they get rough 🌊*

Something broken? Don't panic. Here are the most common issues and how to fix them.

---

## 🏗️ Build Issues

### "First compile is slow / downloading something"

This is normal the first time.

Windows downloads:
- Zig compiler (~170 MB)
- Ninja (~285 KB)
- CMake (~50 MB)

Linux uses system GCC/G++ and downloads Ninja/CMake when they are missing.

You'll see log lines like `"Downloading CMake (first-time setup)..."` in the Console so you know it isn't frozen.

**If download fails:**
- Check firewall / antivirus isn't blocking `curl` calls to GitHub and ziglang.org.
- Run `Tools\setup_tools.ps1` (Windows) or `Tools/setup_tools.sh` (Linux)
  manually from a terminal to see the exact error.
- Once downloaded, tools are cached in `Tools/{zig,ninja,cmake}/` (Windows) or `Tools/{ninja,cmake}/` (Linux) — subsequent compiles are fast.

### "Permission denied" when building the editor
*   **The Problem:** You are trying to rebuild the editor (`RayWaves.exe` / `RayWaves`) while it is running.
*   **The Fix:** Close the game window, *then* rebuild.
*   **Note:** You *can* rebuild the GameLogic module while the game is running. That's the whole point!

### "CMake configuration failed"
*   **The Problem:** You might be using the wrong CMake preset or missing the platform compiler.
*   **The Fix:** Use `cmake --preset zig-debug`. Windows fetches Zig automatically; Linux requires `build-essential` (GCC/G++).

### Linux: missing GCC/G++
*   **The Problem:** CMake reports that GCC/G++ was not found.
*   **The Fix:** `sudo apt install build-essential`. Linux hot reload requires the
    shared system libstdc++ runtime.

### Linux: missing X11 / GL / ALSA headers
*   **The Problem:** raylib cannot find `X11/Xlib.h`, `GL/gl.h`, or `alsa/asoundlib.h`.
*   **The Fix:** Install the development packages listed in
    [linux.md](linux.md), then delete the build directory and configure again.

### Linux: `cannot open shared object file: libraylib.so.600`
*   **The Problem:** the runtime cannot find the raylib shared library.
*   **The Fix:** run from the build/dist directory that contains `libraylib.so*`,
    or reinstall the package. The binaries use an `$ORIGIN` RPATH.

### "lld-link: error: duplicate symbol" or `std::bad_function_call` errors
*   **The Problem:** The module and the engine were built with different toolchains, or a stale object cache survived a toolchain update. C++ runtime symbols are version-dependent. On Windows this can surface as `lld-link` errors; on Linux it can surface as `std::bad_function_call` or undefined C++ runtime symbols.
*   **The Fix:** Delete the project's `.raywaves/build/` folder and click **Compile** again. The generated project CMake compiles `Engine/*.cpp` sources alongside your `GameLogic/*.cpp`, so both sides use the same compiler and runtime. Do not mix Zig-built and GCC-built objects.
*   **Do not link a precompiled `libEngine.a`.** Dev builds and distributions both use `packaging/Project.CMakeLists.in`; there is no second project build recipe.

---

## 🔥 Hot-Reload Issues

### "I changed the code, but nothing happened!"
1.  **Did the build succeed?** Check the terminal. If there was a syntax error, the DLL wasn't updated.
2.  **Did the timestamp change?** The editor watches for the file timestamp. If the build was too fast or didn't actually write the file, it might be ignored.
3.  **Try forcing it:** Click the **Restart** button (Refresh icon) on the toolbar.

### "Access Violation" / Crash on Reload
*   **The Problem:** You probably have a pointer pointing to old memory, or a static variable that didn't get reset.
*   **The Fix:**
    *   State serialization (`SaveState`/`LoadState`) is opt-in. Pointers and GPU resources must not be serialized, they will reset.
    *   Initialize all variables in `Initialize()`, not just in the constructor.
    *   Avoid global variables in your cpp files if possible.
    *   Check your `Cleanup()` method if you are manually managing memory.

---

## 🎨 Asset & Visual Issues

### Purple/Black Textures (Missing Assets)
*   **The Problem:** Raylib can't find the file.
*   **The Fix:**
    *   Check the path. Is it relative to the editor executable (`RayWaves.exe` on Windows, `RayWaves` on Linux)?
    *   Did you use forward slashes? `"Assets/player.png"` ✅ vs `"Assets\player.png"` ❌
    *   Is the file actually in your project's `Assets/` folder?

### "Text looks blurry" or "Window is tiny"
*   **The Problem:** High-DPI scaling on Windows.
*   **The Fix:**
    *   Check `config.ini` and increase the width/height.
    *   Set `b_Fullscreen=true` for a simplified view.

---

## 📦 Export Issues

### Hyprland: the editor opens floating and looks small
*   **The Problem:** Hyprland can float XWayland windows, and a 1280x720 request
    becomes 1024x576 logical at a 1.25 monitor scale.
*   **The Fix:** the editor creates its window resizable, advertises a normal
    X11 window type, sets explicit size bounds, and sizes it to roughly 90% of
    the monitor. If it still floats, add a Hyprland rule:
    ```lua
    hl.window_rule({ match = { class = "^(RayWaves)$" }, float = false })
    ```
    See [linux.md](linux.md#hyprland).

### Linux: file dialogs do nothing
*   **The Problem:** `tinyfiledialogs` needs a native dialog backend.
*   **The Fix:** install `zenity` (GNOME) or `kdialog` (KDE).

### "The exported game crashes immediately"
*   **The Problem:** Usually missing assets or config files.
*   **The Fix:**
    *   Go to your exported folder.
    *   Make sure the `Assets/` folder is there.
    *   Make sure `config.ini` is there.
    *   Try running it from a terminal to see if it prints an error message before dying.

---

## 🧠 Still Stuck?

1.  **Clean Rebuild:** Sometimes CMake cache gets weird. Delete the `out/` or `build/` folder and try again.
2.  **Check Console:** The engine prints extensive logs to the terminal window. Read them!
3.  **Simplify:** Comment out the last thing you added. Does it work now?

*Good luck!* 🛠️