# Distribution Files

This folder contains all the files needed for creating distribution packages of the RayWaves Game Engine.

## Files

- **`build_gamelogic.bat`** - Quick build script for GameLogic.dll hot-reloading
- **`create_distribution.bat`** - Main distribution creation script
- **`dist_CMakeLists.txt`** - CMake configuration for distributed development environment
- **`distribute.ps1`** - PowerShell script that handles the actual packaging
- **`game_config.ini`** - Default game configuration template

## Usage

### Creating a Distribution

Run from the project root directory:

```bash
Distribution\create_distribution.bat
```

This will:
1. Build the release version of the engine and game logic
2. Create a `dist` folder with all necessary files
3. Copy all required assets and documentation
4. Set up a complete development environment for end users

### Distribution Structure

The created distribution includes:
- **`app.exe`** - The main game engine/editor executable
- **`GameLogic.dll`** - Hot-reloadable game logic
- **`raylib.dll`** - Graphics library dependency
- **`game_config.ini`** - Game configuration file
- **`build_gamelogic.bat`** - Build script for users to modify game logic
- **`CMakeLists.txt`** - CMake configuration for building GameLogic
- **`raylib/`** - Raylib development files (headers, libs)
- **`GameLogic/`** - Source code for game logic
- **`Engine/`** - Engine headers for development
- **`Assets/`** - Game assets (excluding EngineContent)
- **`Documentation/`** - User guides and API reference

## Customization

- **Output Directory**: Modify `distribute.ps1` or pass parameters to change where the distribution is created.
- **Compiler Bundling**: Pass `-IncludeCompiler` to `distribute.ps1` if you want to bundle the Zig compiler for developers (by default, it produces a lean build for players).
- **Build Configuration**: Use Debug or Release builds by modifying the scripts.
- **Asset Filtering**: Edit `distribute.ps1` to change which assets are included.

## Notes

- The distribution is self-contained and doesn't require the original source project
- Users can develop game logic using the provided build script
- All necessary development tools and headers are included
- The editor and game runtime are combined in `app.exe`