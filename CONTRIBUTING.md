# Contributing

## Documentation Checklist

When making code changes, update docs too:

1. **Changed folder structure?** Update `Documentation/ARCHITECTURE.md`.
2. **Renamed an exe or file?** `grep -rn "oldname" Documentation/ README.md` before committing — update every hit.
3. **Removed a UI feature?** `grep -rn -i "featurename" Documentation/ README.md` and remove stale references.
4. **Changed build steps or toolchain?** Update *First-Time Compile* section in `DEVELOPER_GUIDE.md` and any related troubleshooting entries.
