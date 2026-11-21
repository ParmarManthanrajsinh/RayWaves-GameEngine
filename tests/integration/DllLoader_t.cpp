#include "Editor/GameLogicLoader.h"
#include "Engine/GameEngine.h"
#include "Engine/GameMap.h"
#include "Engine/PlatformNames.h"
#include "PlatformInfo.h"
#include "ProcessRunner.h"
#include "Runtime/DllLoader.h"
#include "doctest/doctest.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
// Repo root derived from this executable's location:
// <repo>/build/<config>/<test>.exe -> repo root is three levels up.
// (CWD is unreliable: ctest uses the binary dir, manual runs may not.)
fs::path GetRepoRoot()
{
    std::string exe_path = GetHostExePath();
    fs::path dir = fs::path(exe_path).parent_path();
    return dir.parent_path().parent_path();
}

// Minimal test DLL source exporting the required GameLogic ABI.
// Built with -I<repo root>, so includes are repo-relative.
constexpr const char* TEST_DLL_SOURCE = R"SRC(
#include "Engine/GameLogicExports.h"
#include "Engine/GameMap.h"
#include <cstdint>
#include <cstdlib>
#include <fstream>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
    void SaveState(StateBag& out) const override { out.SetFloat("value", 42.0f); }
    void LoadState(const StateBag& in) override { (void)in.GetInt("value", 0); }
};
RW_GAMELOGIC_EXPORT GameMap* CreateGameMap() { return new TestMap(); }
RW_GAMELOGIC_EXPORT void DestroyGameMap(GameMap* m) {
    if (const char* marker = std::getenv("RAYWAVES_TEST_DLL_DESTROY_MARKER")) {
        std::ofstream(marker) << "destroyed";
    }
    delete m;
}
RW_GAMELOGIC_EXPORT uint32_t GetGameLogicAbiVersion() { return RAYWAVES_GAMELOGIC_ABI_VERSION; }
)SRC";

// Copy of the source with a WRONG ABI version, used for mismatch tests
constexpr const char* TEST_DLL_BAD_ABI = R"SRC(
#include "Engine/GameLogicExports.h"
#include "Engine/GameMap.h"
#include <cstdint>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
};
RW_GAMELOGIC_EXPORT GameMap* CreateGameMap() { return new TestMap(); }
RW_GAMELOGIC_EXPORT void DestroyGameMap(GameMap* m) { delete m; }
RW_GAMELOGIC_EXPORT uint32_t GetGameLogicAbiVersion() { return 0xDEAD; }
)SRC";

// Windows test modules use the pinned Zig toolchain. Linux uses the same
// system G++/libstdc++ as the host so STL objects can cross the module
// boundary safely.
fs::path repo_root = GetRepoRoot();
fs::path test_compiler = rw::platform::b_IsWindows ? (repo_root / "Tools" / "zig" / rw::platform::ExecutableName("zig"))
                                                   : PlatformInfo::FindExecutable("g++");
// Staged raylib root next to the test executable, with a fallback to the
// FetchContent build tree. The staged lib/ directory is only populated by
// a configure pass that runs after raylib has been built, so on a fresh
// build directory it can be missing entirely.
fs::path build_dir = fs::path(GetHostExePath()).parent_path();
fs::path raylib_staged = build_dir / "raylib";
fs::path raylib_fetched = build_dir / "_deps" / "raylib-build" / "raylib";

fs::path ResolveRaylibInclude()
{
    for (const auto& candidate : { raylib_staged / "include", raylib_fetched / "include" })
    {
        if (fs::exists(candidate))
            return candidate;
    }
    return raylib_staged / "include";
}

// Zig looks for raylib.dll, raylib.lib, libraylib.a, or libraylib.so. Point
// the compiler at the staged file directly so the import-name rules do not
// matter.
fs::path ResolveRaylibImportLib()
{
    for (const auto& candidate : {
             raylib_staged / "lib" / "libraylib.a",
             raylib_staged / "lib" / "libraylib.dll.a",
             raylib_staged / "lib" / "libraylib.so",
             raylib_fetched / "libraylib.dll.a",
             raylib_fetched / "libraylib.a",
             raylib_fetched / "libraylib.so",
         })
    {
        if (fs::exists(candidate))
            return candidate;
    }
    return {};
}

std::string WriteTestDll(const char* source, const std::string& tag)
{
    fs::path tmp = fs::temp_directory_path() / ("raywaves_tests" + tag);
    fs::create_directories(tmp);
    fs::path cpp = tmp / "TestGameLogic.cpp";
    {
        std::ofstream ofs(cpp);
        ofs << source;
    }

    if (test_compiler.empty() || !fs::exists(test_compiler))
        return "";

    fs::path raylib_include = ResolveRaylibInclude();
    fs::path raylib_import = ResolveRaylibImportLib();
    if (!fs::exists(raylib_import))
        return "";

    // GameMap isn't header-only; compile the minimal Engine translation
    // units it depends on (same approach the export pipeline uses).
    const fs::path output = tmp / rw::platform::SharedLibraryName("TestGameLogic");
    std::vector<std::string> argv = { test_compiler.string() };
    if (rw::platform::b_IsWindows)
    {
        argv.push_back("c++");
    }
    else
    {
        argv.push_back("-fPIC");
        argv.push_back("-fno-gnu-unique");
    }
    argv.insert(argv.end(),
        { "-shared", "-std=c++23", "-o", output.string(), cpp.string(), (repo_root / "Engine" / "GameMap.cpp").string(),
            (repo_root / "Engine" / "AssetResolver.cpp").string(), "-I" + repo_root.string(),
            "-I" + raylib_include.string(), raylib_import.string() });

    std::vector<std::string> output_lines;
    const bool b_ok = ProcessRunner::RunAndWaitCommand(
        argv, repo_root.string(), [&output_lines](std::string_view line, bool) { output_lines.emplace_back(line); });
    if (!b_ok)
    {
        for (const std::string& line : output_lines)
        {
            std::cerr << line << "\n";
        }
        return "";
    }
    return output.string();
}
} // namespace

TEST_CASE("DllLoader: shadow copy load, symbol resolve, unload")
{
    const fs::path shadow_root = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_root);

    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_ok");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);

    // Must be a shadow copy, not the original file, and directly in the root.
    CHECK(fs::path(dll.shadow_path).filename().string().find(".shadow.") != std::string::npos);
    CHECK(fs::path(dll.shadow_path).parent_path() == shadow_root);
    CHECK(fs::path(dll.shadow_path).string() != dll_path);

    // Symbols resolve
    REQUIRE(GetDllSymbol(dll, "CreateGameMap") != nullptr);
    REQUIRE(GetDllSymbol(dll, "DestroyGameMap") != nullptr);
    REQUIRE(GetDllSymbol(dll, "GetGameLogicAbiVersion") != nullptr);
    CHECK(GetDllSymbol(dll, "NoSuchSymbol") == nullptr);

    // ABI version matches the engine
    auto abi = reinterpret_cast<uint32_t (*)()>(GetDllSymbol(dll, "GetGameLogicAbiVersion"));
    CHECK(abi() == RAYWAVES_GAMELOGIC_ABI_VERSION);

    // Factory produces a working map
    auto create = reinterpret_cast<GameMap* (*)()>(GetDllSymbol(dll, "CreateGameMap"));
    auto destroy = reinterpret_cast<void (*)(GameMap*)>(GetDllSymbol(dll, "DestroyGameMap"));
    GameMap* map = create();
    REQUIRE(map != nullptr);
    CHECK(map->GetMapName() == "TestMap");
    CHECK(map->b_IsMapManager() == false);
    StateBag bag;
    map->SaveState(bag);
    CHECK(bag.GetFloat("value") == doctest::Approx(42.0f));
    destroy(map);

    // Unload frees handle and deletes the shadow copy
    std::string shadow = dll.shadow_path;
    UnloadDll(dll);
    CHECK(dll.handle == nullptr);
    CHECK(!fs::exists(shadow));
}

TEST_CASE("GameLogicLoader: unload destroys a plain DLL-backed GameMap")
{
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_loader_destroy");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
        return;
    }

    const fs::path marker = fs::temp_directory_path() / "raywaves_loader_destroy_marker.txt";
    fs::remove(marker);
    REQUIRE(PlatformInfo::b_SetEnvironmentVariable("RAYWAVES_TEST_DLL_DESTROY_MARKER", marker.string()));

    {
        GameEngine engine;
        GameLogicLoader loader(engine);
        REQUIRE(loader.b_LoadGameLogic(dll_path));
        REQUIRE(engine.GetMap() != nullptr);

        loader.Unload();
        CHECK(engine.GetMap() == nullptr);
        CHECK(fs::exists(marker));
    }

    PlatformInfo::b_SetEnvironmentVariable("RAYWAVES_TEST_DLL_DESTROY_MARKER", "");
    fs::remove(marker);
}

TEST_CASE("DllLoader: unload keeps original DLL file writable and intact")
{
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_writable");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);

    // Original file must still exist and be re-writable (shadow keeps it unlocked)
    CHECK(fs::exists(dll_path));
    {
        std::ofstream ofs(dll_path, std::ios::app);
        CHECK(ofs.good());
    }

    UnloadDll(dll);
}

TEST_CASE("DllLoader: missing file falls back to direct load and fails gracefully")
{
    const std::string missing = rw::platform::SharedLibraryName("definitely_missing_dll_12345");
    DllHandle dll = LoadDll(missing.c_str());
    CHECK(dll.handle == nullptr);
    CHECK(GetDllSymbol(dll, "CreateGameMap") == nullptr);
    // Unload on a null handle must not crash
    UnloadDll(dll);
}

TEST_CASE("DllLoader: bad ABI version DLL still loads (editor rejects it)")
{
    // LoadDll itself only guards CRT compatibility; the ABI version check
    // happens in the editor/runtime loader layer (b_LoadGameLogic).
    // Here we verify the bad-ABI DLL loads and its wrong version is readable.
    std::string dll_path = WriteTestDll(TEST_DLL_BAD_ABI, "_badabi");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);
    auto abi = reinterpret_cast<uint32_t (*)()>(GetDllSymbol(dll, "GetGameLogicAbiVersion"));
    REQUIRE(abi != nullptr);
    CHECK(abi() != RAYWAVES_GAMELOGIC_ABI_VERSION); // mismatch detectable
    UnloadDll(dll);
}

TEST_CASE("DllLoader: CRT import parsing on self and a real module")
{
#if defined(_WIN32)
    // The running test executable itself uses the dynamic CRT; parse its imports.
    std::string exe_path = GetHostExePath();
    REQUIRE(!exe_path.empty());
    std::vector<std::string> exe_imports = GetModuleCrtImports(exe_path.c_str());
    CHECK(!exe_imports.empty());

    // A module built with the same toolchain must be compatible
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_crt");
    if (dll_path.empty())
    {
        FAIL("Failed to build test module (zig not on PATH?)");
        return;
    }
    std::vector<std::string> dll_imports = GetModuleCrtImports(dll_path.c_str());
    CHECK(b_CrtImportsCompatible(exe_imports, dll_imports));

    // Static-vs-static is also fine
    CHECK(b_CrtImportsCompatible({}, {}));

    // Static-vs-dynamic is rejected
    CHECK_FALSE(b_CrtImportsCompatible(exe_imports, {}));
    CHECK_FALSE(b_CrtImportsCompatible({}, exe_imports));

    // Nonexistent module returns empty without crashing
    CHECK(GetModuleCrtImports("definitely_missing_dll_12345.dll").empty());
#else
    // POSIX has one process-wide C++ runtime; the PE import scan is a no-op.
    const std::string missing = rw::platform::SharedLibraryName("definitely_missing_dll_12345");
    CHECK(GetModuleCrtImports(missing.c_str()).empty());
    CHECK(b_CrtImportsCompatible({}, {}));
    CHECK(b_CrtImportsCompatible({ "anything" }, {}));
#endif
}

TEST_CASE("DllLoader: CleanupStaleShadowCopies removes only old shadow DLLs")
{
    fs::path shadow_dir = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_dir);

    const std::string suffix = rw::platform::c_SharedLibrarySuffix;
    fs::path stale = shadow_dir / ("GameLogic.shadow.123.456" + suffix);
    fs::path fresh = shadow_dir / ("GameLogic.shadow.123.999" + suffix);
    fs::path foreign = shadow_dir / ("unrelated" + suffix);

    {
        std::ofstream(stale) << "x";
    }
    {
        std::ofstream(fresh) << "x";
    }
    {
        std::ofstream(foreign) << "x";
    }
    REQUIRE(fs::exists(stale));
    REQUIRE(fs::exists(fresh));
    REQUIRE(fs::exists(foreign));

    // Older builds created shadows inside random subdirectories. The sweep
    // must find those files and remove empty leftovers.
    fs::path nested_dir = shadow_dir / "123456789";
    fs::create_directories(nested_dir);
    fs::path nested_stale = nested_dir / ("GameLogic.shadow.7.8.9" + suffix);
    {
        std::ofstream(nested_stale) << "x";
    }
    REQUIRE(fs::exists(nested_stale));

    fs::path empty_dir = shadow_dir / "empty-leftover";
    fs::create_directories(empty_dir);

    // Backdate stale copies by 2 hours
    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);
    fs::last_write_time(nested_stale, old_time);

    CleanupStaleShadowCopies();

    CHECK(!fs::exists(stale)); // old shadow copy removed
    CHECK(!fs::exists(nested_stale)); // nested old shadow copy removed
    CHECK(!fs::exists(nested_dir)); // empty nested directory removed
    CHECK(!fs::exists(empty_dir)); // empty leftover directory removed
    CHECK(fs::exists(fresh)); // recent shadow copy kept
    CHECK(fs::exists(foreign)); // non-shadow file untouched

    fs::remove(fresh);
    fs::remove(foreign);
}

TEST_CASE("DllLoader: project-local shadow cleanup only removes stale project copies")
{
    const fs::path project = fs::temp_directory_path() / "raywaves_project_shadow_cleanup";
    const fs::path shadow_dir = project / ".raywaves" / "shadows";
    fs::remove_all(project);
    fs::create_directories(shadow_dir);

    const std::string suffix = rw::platform::c_SharedLibrarySuffix;
    const fs::path stale = shadow_dir / ("GameLogic.shadow.1.2" + suffix);
    const fs::path fresh = shadow_dir / ("GameLogic.shadow.1.3" + suffix);
    const fs::path foreign = shadow_dir / ("not-a-shadow" + suffix);
    std::ofstream(stale) << "x";
    std::ofstream(fresh) << "x";
    std::ofstream(foreign) << "x";

    const auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);

    CleanupStaleShadowCopies(project);

    CHECK(!fs::exists(stale));
    CHECK(fs::exists(fresh));
    CHECK(fs::exists(foreign));

    fs::remove_all(project);
}

TEST_CASE("DllLoader: log sink captures diagnostics, reset restores stderr default")
{
    std::vector<std::string> captured_errors;
    std::vector<std::string> captured_infos;
    SetDllLogSink(
        [&](std::string_view message, bool is_error)
        {
            if (is_error)
                captured_errors.emplace_back(message);
            else
                captured_infos.emplace_back(message);
        });

    // Plant a stale shadow copy so the sweep has something to report
    fs::path shadow_dir = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_dir);
    fs::path stale = shadow_dir / ("GameLogic.shadow.9.9" + std::string(rw::platform::c_SharedLibrarySuffix));
    {
        std::ofstream(stale) << "x";
    }
    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);

    CleanupStaleShadowCopies();

    CHECK(!fs::exists(stale));
    REQUIRE(captured_infos.size() == 1);
    CHECK(captured_infos[0].find("stale shadow") != std::string::npos);
    CHECK(captured_errors.empty());

    // Reset to the stderr default; must not crash and must accept new sinks after
    SetDllLogSink(nullptr);
    SetDllLogSink(
        [&](std::string_view message, bool is_error)
        {
            (void)message;
            (void)is_error;
        });
    SetDllLogSink(nullptr);
}

TEST_CASE("DllLoader: FormatAbiMismatchMessage distinguishes missing vs mismatched")
{
    std::string missing = FormatAbiMismatchMessage(false, 0, RAYWAVES_GAMELOGIC_ABI_VERSION);
    CHECK(missing.find("missing GetGameLogicAbiVersion") != std::string::npos);
    CHECK(missing.find("Rebuild GameLogic") != std::string::npos);

    std::string mismatch = FormatAbiMismatchMessage(true, 0xDEAD, RAYWAVES_GAMELOGIC_ABI_VERSION);
    CHECK(mismatch.find("ABI mismatch") != std::string::npos);
    CHECK(mismatch.find("57005") != std::string::npos); // 0xDEAD reported DLL version
    CHECK(mismatch.find(std::to_string(RAYWAVES_GAMELOGIC_ABI_VERSION)) != std::string::npos);
    CHECK(mismatch.find("Rebuild GameLogic") != std::string::npos);
}
