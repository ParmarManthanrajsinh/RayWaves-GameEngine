#pragma once

#include <string>
#include <vector>

// PE import-table inspection for CRT compatibility checks. Parses a module
// (EXE or DLL) from disk without loading it, so the loader can refuse
// static-vs-dynamic CRT mixes before they corrupt the heap across the DLL
// boundary. All functions are safe to call with missing/malformed files
// (they yield empty results, never crash).

// CRT DLL names a module imports (e.g. "ucrtbase.dll",
// "api-ms-win-crt-heap-l1-1-0.dll"). Empty on parse failure.
std::vector<std::string> GetModuleCrtImports(const char* module_path);

// True when host EXE and DLL import the same CRT flavor (both dynamic or
// both static). A static-vs-dynamic mix corrupts the heap the moment a
// std::string/std::unordered_map (StateBag) crosses the boundary.
bool b_CrtImportsCompatible(const std::vector<std::string>& exe_imports,
                            const std::vector<std::string>& dll_imports);
