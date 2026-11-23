# Architecture Decision Records

Short records of decisions that shape the codebase. Add a new file for each
decision that would be expensive to reverse.

Format: context, decision, consequences.

- [0001 — Engine as a static library](0001-engine-static-library.md)
- [0002 — GameLogic as a hot-reloaded DLL](0002-gamelogic-hot-reload-dll.md)
- [0003 — One project CMake template](0003-single-project-cmake-template.md)
- [0004 — Benchmarks separate from tests](0004-benchmarks-separate.md)
- [0005 — A small game-facing API (`rw`)](0005-simple-game-api.md)
- [0006 — Editor-owned 3D camera](0006-editor-3d-camera.md)
- [0010 — Agent-facing CLI (`rwcli`)](0010-agent-cli.md)
- [0011 — Split and extend the `rw` game-facing API](0011-rw-facade-headers.md)
- [0007 — Project agent bundle and IDE/LSP files](0007-agent-template-and-lsp.md)
- [0008 — Linux support and a shared platform layer](0008-linux-support.md)
- [0009 — Linux shared C++ runtime and safe module unload](0009-linux-shared-cxx-runtime.md)
