# Troubleshooting Guide

*Fixing the waves when they get rough 🌊*

Something broken? Don't panic. Here are the most common issues and how to fix them.

---

## 🏗️ Build Issues

### "First compile is slow"

The first build compiles the whole engine from source. Later builds are incremental, so they are fast.

RayWaves uses the system toolchain — nothing is downloaded or bundled:
- C++ compiler (GCC or Clang)
- CMake
- Ninja

**If configuration fails:**
- Check that `cmake`, `ninja`, and your compiler are on `PATH`.
- Run `make help` to list the available build targets.

### "Text file busy" / "Permission denied" when building RayWaves
*   **The Problem:** You are trying to rebuild the *engine core* (`RayWaves`) while it is running. Linux will not let you overwrite a running executable.
*   **The Fix:** Close the editor window, *then* rebuild.
*   **Note:** You *can* rebuild `GameLogic.so` while the game is running. That's the whole point!

### "CMake configuration failed"
*   **The Problem:** You might be using the wrong CMake preset or your compiler is not installed.
*   **The Fix:** Ensure you are using `cmake --preset linux-debug` (or just `make dev`). The build uses your system GCC/Clang — no IDE toolchain required!

### "duplicate symbol" or `std::bad_function_call` errors
*   **The Problem:** You are trying to link `GameLogic.so` built with one compiler against a precompiled `libEngine.a` built by another. `libstdc++`/`libc++` symbols are highly version-dependent.
*   **The Fix:** This should not happen if you use the editor's **Compile** button (or the generated `.raywaves/CMakeLists.txt`), which compiles the `Engine/*.cpp` source files alongside your `GameLogic/*.cpp` files in a single invocation to guarantee ABI compatibility.
*   **Known Limitation:** If you link a prebuilt `libEngine.a` yourself instead of using the project build, make sure it comes from the same compiler and C++ standard library, or you remain exposed to this cross-version ABI mismatch bug!

---

## 🔥 Hot-Reload Issues

### "I changed the code, but nothing happened!"
1.  **Did the build succeed?** Check the terminal. If there was a syntax error, the shared library wasn't updated.
2.  **Did the timestamp change?** The editor watches for the file timestamp. If the build was too fast or didn't actually write the file, it might be ignored.
3.  **Try forcing it:** Click the **Restart** button (Refresh icon) on the toolbar.

### "Segmentation fault" / Crash on Reload
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
    *   Check the path. Is it relative to the game binary?
    *   Did you use forward slashes? `"Assets/player.png"` ✅ vs `"Assets\player.png"` ❌
    *   Is the file actually in the export's `Assets` folder?

### "Text looks blurry" or "Window is tiny"
*   **The Problem:** High-DPI display scaling.
*   **The Fix:**
    *   Check `config.ini` and increase the width/height.
    *   Set `b_Fullscreen=true` for a simplified view.

---

## 📦 Export Issues

### "The exported game crashes immediately"
*   **The Problem:** Usually missing assets or config files.
*   **The Fix:**
    *   Go to your exported folder.
    *   Make sure `Assets` folder is there.
    *   Make sure `config.ini` is there.
    *   Try running `./run.sh` from a terminal to see if it prints an error message before dying.

---

## 🧠 Still Stuck?

1.  **Clean Rebuild:** Sometimes the CMake cache gets weird. Delete the `build/` folder (for a project: `.raywaves/build/`) and try again.
2.  **Check Console:** The engine prints extensive logs to the terminal window. Read them!
3.  **Simplify:** Comment out the last thing you added. Does it work now?

*Good luck!* 🛠️