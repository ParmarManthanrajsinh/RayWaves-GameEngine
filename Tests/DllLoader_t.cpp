#include "doctest/doctest.h"
#include "../Game/DllLoader.h"
#include "../Engine/GameMap.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    // Repo root derived from this executable's location:
    // <repo>/build/<config>/tests.exe -> repo root is three levels up.
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
#include "Engine/GameMap.h"
#include <cstdint>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
    void SaveState(StateBag& out) const override { out.SetFloat("value", 42.0f); }
    void LoadState(const StateBag& in) override { (void)in.GetInt("value", 0); }
};
extern "C" __declspec(dllexport) GameMap* CreateGameMap() { return new TestMap(); }
extern "C" __declspec(dllexport) void DestroyGameMap(GameMap* m) { delete m; }
extern "C" __declspec(dllexport) uint32_t GetGameLogicAbiVersion() { return RAYWAVES_GAMELOGIC_ABI_VERSION; }
)SRC";

    // Copy of the source with a WRONG ABI version, used for mismatch tests
    constexpr const char* TEST_DLL_BAD_ABI = R"SRC(
#include "Engine/GameMap.h"
#include <cstdint>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
};
extern "C" __declspec(dllexport) GameMap* CreateGameMap() { return new TestMap(); }
extern "C" __declspec(dllexport) void DestroyGameMap(GameMap* m) { delete m; }
extern "C" __declspec(dllexport) uint32_t GetGameLogicAbiVersion() { return 0xDEAD; }
)SRC";

    // Locate the repo-bundled zig relative to the test executable:
    // <repo>/build/<config>/tests.exe
    fs::path repo_root = GetRepoRoot();
    fs::path zig_exe = repo_root / "Tools" / "zig" / "zig.exe";
    // Staged raylib headers next to the build dir (CMake copies them there)
    fs::path raylib_include = fs::path(GetHostExePath()).parent_path() / "raylib" / "include";
    fs::path raylib_lib = fs::path(GetHostExePath()).parent_path() / "raylib" / "lib";

    std::string WriteTestDll(const char* source, const std::string& tag)
    {
        fs::path tmp = fs::temp_directory_path() / ("raywaves_tests" + tag);
        fs::create_directories(tmp);
        fs::path cpp = tmp / "TestGameLogic.cpp";
        { std::ofstream ofs(cpp); ofs << source; }

        if (!fs::exists(zig_exe)) return "";

        // GameMap isn't header-only; compile the minimal Engine translation
        // units it depends on (same approach the export pipeline uses).
        std::string cmd = "\"" + zig_exe.string() + "\" c++ -shared -std=c++23 -o "
            + (tmp / "TestGameLogic.dll").string()
            + " " + cpp.string()
            + " " + (repo_root / "Engine" / "GameMap.cpp").string()
            + " " + (repo_root / "Engine" / "AssetResolver.cpp").string()
            + " -I" + repo_root.string()
            + " -I" + raylib_include.string()
            + " -L" + raylib_lib.string()
            + " -lraylib";
        if (std::system(cmd.c_str()) != 0) return "";
        return (tmp / "TestGameLogic.dll").string();
    }
}

TEST_CASE("DllLoader: shadow copy load, symbol resolve, unload")
{
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_ok");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);

    // Must be a shadow copy, not the original file
    CHECK(fs::path(dll.shadow_path).filename().string().find(".shadow.") != std::string::npos);
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
    auto create = reinterpret_cast<GameMap * (*)()>(GetDllSymbol(dll, "CreateGameMap"));
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
    { std::ofstream ofs(dll_path, std::ios::app); CHECK(ofs.good()); }

    UnloadDll(dll);
}

TEST_CASE("DllLoader: missing file falls back to direct load and fails gracefully")
{
    DllHandle dll = LoadDll("definitely_missing_dll_12345.dll");
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

TEST_CASE("DllLoader: CRT import parsing on self and a real DLL")
{
    // The running tests.exe itself uses the dynamic CRT; parse its imports.
    std::string exe_path = GetHostExePath();
    REQUIRE(!exe_path.empty());
    std::vector<std::string> exe_imports = GetModuleCrtImports(exe_path.c_str());
    CHECK(!exe_imports.empty());

    // A DLL built with the same toolchain must be compatible
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_crt");
    if (dll_path.empty())
    {
        FAIL("Failed to build test DLL (zig not on PATH?)");
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
}

TEST_CASE("DllLoader: CleanupStaleShadowCopies removes only old shadow DLLs")
{
    fs::path shadow_dir = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_dir);

    fs::path stale = shadow_dir / "GameLogic.shadow.123.456.dll";
    fs::path fresh = shadow_dir / "GameLogic.shadow.123.999.dll";
    fs::path foreign = shadow_dir / "unrelated.dll";

    { std::ofstream(stale) << "x"; }
    { std::ofstream(fresh) << "x"; }
    { std::ofstream(foreign) << "x"; }
    REQUIRE(fs::exists(stale));
    REQUIRE(fs::exists(fresh));
    REQUIRE(fs::exists(foreign));

    // Backdate 'stale' by 2 hours
    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);

    CleanupStaleShadowCopies();

    CHECK(!fs::exists(stale));   // old shadow copy removed
    CHECK(fs::exists(fresh));   // recent shadow copy kept
    CHECK(fs::exists(foreign)); // non-shadow file untouched

    fs::remove(fresh);
    fs::remove(foreign);
}

TEST_CASE("DllLoader: log sink captures diagnostics, reset restores stderr default")
{
    std::vector<std::string> captured_errors;
    std::vector<std::string> captured_infos;
    SetDllLogSink([&](std::string_view message, bool is_error)
    {
        if (is_error) captured_errors.emplace_back(message);
        else captured_infos.emplace_back(message);
    });

    // Plant a stale shadow copy so the sweep has something to report
    fs::path shadow_dir = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_dir);
    fs::path stale = shadow_dir / "GameLogic.shadow.9.9.dll";
    { std::ofstream(stale) << "x"; }
    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);

    CleanupStaleShadowCopies();

    CHECK(!fs::exists(stale));
    REQUIRE(captured_infos.size() == 1);
    CHECK(captured_infos[0].find("stale shadow") != std::string::npos);
    CHECK(captured_errors.empty());

    // Reset to the stderr default; must not crash and must accept new sinks after
    SetDllLogSink(nullptr);
    SetDllLogSink([&](std::string_view message, bool is_error)
    {
        (void)message; (void)is_error;
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
