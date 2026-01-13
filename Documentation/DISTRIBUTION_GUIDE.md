# RayWaves Distribution Guide

*Where code changes flow like waves 🌊*

This guide explains how to package the RayWaves engine so OTHER developers can make games with it.

> **Note:** If you just want to *export your game* to play on another computer, check the [Developer Guide](DEVELOPER_GUIDE.md). This guide is for distributing the *engine tools* themselves.

---

## 🚀 The "Two-Click" Distribution

Want to give a friend the entire engine so they can start coding?

1.  **Run the Script:**
    ```cmd
    Distribution\create_distribution.bat
    ```
2.  **Wait:** The script will build the engine, gather assets, and clean up headers.
3.  **Done!** Check the newly created `dist/` folder.

You can now zip up `dist/` and send it to anyone!

---

## 📦 What's Inside the Box?

When you create a distribution, here is what your users get:

| File/Folder | Purpose |
|-------------|---------|
| `editor.exe` | The visual game editor (renamed `app.exe` in some versions). |
| `GameLogic.dll` | The compiled game code. |
| `game_config.ini` | Default settings (resolution, VSync, etc). |
| `build_gamelogic.bat` | **The Magic Button.** Users click this to recompile their code. |
| `GameLogic/` | **User Code.** Contains `Level1.cpp`, `Player.cpp`, etc. |
| `Engine/` | **Header Files.** So users can define classes inheriting `GameMap`. |
| `Assets/` | **Resources.** Textures, sounds, and fonts. |

---

## 🎮 The End-User Experience

Imagine you send this to a friend. Here is their workflow:

1.  **Unzip** the folder.
2.  **Open `GameLogic/Level1.cpp`** in VS Code (or Notepad++).
3.  **Run `editor.exe`**. They see the game running.
4.  **Edit `Level1.cpp`** (e.g., change `speed = 100` to `speed = 500`).
5.  **Run `build_gamelogic.bat`**.
6.  **BOOM!** The editor hot-reloads the new speed instantly.

They **do not** need Visual Studio installed. They just need the build tools (which `build_gamelogic.bat` finds automatically).

---

## 🛠️ Customizing the Distro

You can tweak how the distribution is built by editing `Distribution/distribute.ps1`.

### Common Customizations

*   **Change Default Config:** Modify `Distribution/config.ini` to set different starting resolutions or flags.
*   **Include Extra Assets:** Add files to `GameLogic/` before building if you want to include starter scripts.
*   **Branding:** Change the icon or name of `editor.exe` in the script.

---

## ✅ Checklist Before Shipping

1.  **Test on a generic PC:** Try running the requested `dist/` folder on a computer that *doesn't* have your full dev environment.
2.  **Check Hot-Reload:** Ensure `build_gamelogic.bat` actually triggers a reload in the editor.
3.  **Docs:** Make sure `README_DISTRIBUTION.md` (which becomes the user's `README.md`) covers the basics.

---

*Now go share your engine with the world! 🌍*
