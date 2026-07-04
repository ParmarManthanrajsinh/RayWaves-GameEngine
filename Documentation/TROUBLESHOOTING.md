# Troubleshooting Guide

*Fixing the waves when they get rough 🌊*

Something broken? Don't panic. Here are the most common issues and how to fix them.

---

## 🏗️ Build Issues

### "Permission denied" when building RayWaves.exe
*   **The Problem:** You are trying to rebuild the *engine core* (`RayWaves.exe`) while it is running.
*   **The Fix:** Close the game window, *then* rebuild.
*   **Note:** You *can* rebuild `GameLogic.dll` while the game is running. That's the whole point!

### "CMake configuration failed"
*   **The Problem:** You might be using the wrong CMake preset or missing the Zig compiler.
*   **The Fix:** Ensure you are using `cmake --preset zig-debug`. The engine uses the Zig compiler which is fetched automatically on your first build, so you **do not** need Visual Studio installed!

### "lld-link: error: duplicate symbol" or `std::bad_function_call` errors
*   **The Problem:** You are trying to link `GameLogic.dll` built with one version of the Zig compiler against a precompiled `libEngine.a` built by a different version of the Zig compiler. `libc++` standard library symbols are highly version-dependent.
*   **The Fix:** This should not happen if you use the provided `build_gamelogic.bat` script, which now compiles the `Engine/*.cpp` source files alongside your `GameLogic/*.cpp` files in a single invocation to guarantee ABI compatibility.
*   **Known Limitation:** The secondary build path (`Core/CMakeLists.txt`) still links the precompiled `libEngine.a` and has not been updated yet. If you use CMake directly to build GameLogic instead of the batch script, you remain exposed to this cross-version ABI mismatch bug!

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
    *   Check the path. Is it relative to `RayWaves.exe`?
    *   Did you use forward slashes? `"Assets/player.png"` ✅ vs `"Assets\player.png"` ❌
    *   Is the file actually in the `dist/Assets` folder?

### "Text looks blurry" or "Window is tiny"
*   **The Problem:** High-DPI scaling on Windows.
*   **The Fix:**
    *   Check `game_config.ini` and increase the width/height.
    *   Set `b_Fullscreen=true` for a simplified view.

---

## 📦 Export Issues

### "The exported game crashes immediately"
*   **The Problem:** Usually missing assets or config files.
*   **The Fix:**
    *   Go to your exported folder.
    *   Make sure `ASSETS` folder is there.
    *   Make sure `game_config.ini` is there.
    *   Try running it from a terminal to see if it prints an error message before dying.

---

## 🧠 Still Stuck?

1.  **Clean Rebuild:** Sometimes CMake cache gets weird. Delete the `out/` or `build/` folder and try again.
2.  **Check Console:** The engine prints extensive logs to the terminal window. Read them!
3.  **Simplify:** Comment out the last thing you added. Does it work now?

*Good luck!* 🛠️