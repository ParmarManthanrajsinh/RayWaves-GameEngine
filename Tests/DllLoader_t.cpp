#include "../Engine/GameMap.h"
#include "../Game/DllLoader.h"
#include "doctest/doctest.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    // Repo root derived from this executable's location:
    // <repo>/build/<config>/tests -> repo root is three levels up.
    // (CWD is unreliable: ctest uses the binary dir, manual runs may not.)
    fs::path GetRepoRoot()
    {
        std::string exe_path = GetHostExePath();
        fs::path dir         = fs::path(exe_path).parent_path();
        return dir.parent_path().parent_path();
    }

    // Minimal test DLL source exporting the required GameLogic ABI.
    // Built with -I<repo root>, so includes are repo-relative.
    constexpr const char *TEST_DLL_SOURCE = R"SRC(
#include "Engine/GameMap.h"
#include <cstdint>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
    void SaveState(StateBag& out) const override { out.SetFloat("value", 42.0f); }
    void LoadState(const StateBag& in) override { (void)in.GetInt("value", 0); }
};
extern "C" GameMap* CreateGameMap() { return new TestMap(); }
extern "C" void DestroyGameMap(GameMap* m) { delete m; }
extern "C" uint32_t GetGameLogicAbiVersion() { return RAYWAVES_GAMELOGIC_ABI_VERSION; }
)SRC";

    // Copy of the source with a WRONG ABI version, used for mismatch tests
    constexpr const char *TEST_DLL_BAD_ABI = R"SRC(
#include "Engine/GameMap.h"
#include <cstdint>
class TestMap : public GameMap {
public:
    TestMap() : GameMap("TestMap") {}
};
extern "C" GameMap* CreateGameMap() { return new TestMap(); }
extern "C" void DestroyGameMap(GameMap* m) { delete m; }
extern "C" uint32_t GetGameLogicAbiVersion() { return 0xDEAD; }
)SRC";

    // Locate the system compiler and staged raylib relative to the test
    // executable: <repo>/build/<config>/tests
    fs::path repo_root = GetRepoRoot();
    fs::path exe_dir   = fs::path(GetHostExePath()).parent_path();
    // Staged raylib headers + shared lib next to the build dir (CMake copies
    // them there; the .so lands in bin/ so the loader finds it via rpath).
    fs::path raylib_include = exe_dir / "raylib" / "include";
    fs::path raylib_bin     = exe_dir / "raylib" / "bin";

    std::string WriteTestDll(const char *source, const std::string &tag)
    {
        fs::path tmp = fs::temp_directory_path() / ("raywaves_tests" + tag);
        fs::create_directories(tmp);
        fs::path cpp = tmp / "TestGameLogic.cpp";
        {
            std::ofstream ofs(cpp);
            ofs << source;
        }

        // GameMap isn't header-only; compile the minimal Engine translation
        // units it depends on (same approach the export pipeline uses).
        std::string cmd =
            "g++ -shared -fPIC -std=c++23 -o " +
            (tmp / "TestGameLogic.so").string() + " " + cpp.string() + " " +
            (repo_root / "Engine" / "GameMap.cpp").string() + " " +
            (repo_root / "Engine" / "AssetResolver.cpp").string() + " -I" +
            repo_root.string() + " -I" + raylib_include.string() + " -L" +
            raylib_bin.string() + " -Wl,-rpath," + raylib_bin.string() +
            " -lraylib";
        if (std::system(cmd.c_str()) != 0)
            return "";
        return (tmp / "TestGameLogic.so").string();
    }
} // namespace

TEST_CASE("DllLoader: shadow copy load, symbol resolve, unload")
{
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_ok");
    if (dll_path.empty())
    {
        FAIL("Failed to build test plugin (g++ not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);

    // Must be a shadow copy, not the original file
    CHECK(fs::path(dll.shadow_path).filename().string().find(".shadow.") !=
          std::string::npos);
    CHECK(fs::path(dll.shadow_path).string() != dll_path);

    // Symbols resolve
    REQUIRE(GetDllSymbol(dll, "CreateGameMap") != nullptr);
    REQUIRE(GetDllSymbol(dll, "DestroyGameMap") != nullptr);
    REQUIRE(GetDllSymbol(dll, "GetGameLogicAbiVersion") != nullptr);
    CHECK(GetDllSymbol(dll, "NoSuchSymbol") == nullptr);

    // ABI version matches the engine
    auto abi = reinterpret_cast<uint32_t (*)()>(
        GetDllSymbol(dll, "GetGameLogicAbiVersion"));
    CHECK(abi() == RAYWAVES_GAMELOGIC_ABI_VERSION);

    // Factory produces a working map
    auto create =
        reinterpret_cast<GameMap *(*)()>(GetDllSymbol(dll, "CreateGameMap"));
    auto destroy = reinterpret_cast<void (*)(GameMap *)>(
        GetDllSymbol(dll, "DestroyGameMap"));
    GameMap *map = create();
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

TEST_CASE("DllLoader: unload keeps original plugin file writable and intact")
{
    std::string dll_path = WriteTestDll(TEST_DLL_SOURCE, "_writable");
    if (dll_path.empty())
    {
        FAIL("Failed to build test plugin (g++ not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);

    // Original file must still exist and be re-writable (shadow keeps it
    // unlocked)
    CHECK(fs::exists(dll_path));
    {
        std::ofstream ofs(dll_path, std::ios::app);
        CHECK(ofs.good());
    }

    UnloadDll(dll);
}

TEST_CASE(
    "DllLoader: missing file falls back to direct load and fails gracefully")
{
    DllHandle dll = LoadDll("definitely_missing_plugin_12345.so");
    CHECK(dll.handle == nullptr);
    CHECK(GetDllSymbol(dll, "CreateGameMap") == nullptr);
    // Unload on a null handle must not crash
    UnloadDll(dll);
}

TEST_CASE("DllLoader: bad ABI version DLL still loads (editor rejects it)")
{
    // LoadDll itself does not check the ABI version; the check
    // happens in the editor/runtime loader layer (b_LoadGameLogic).
    // Here we verify the bad-ABI DLL loads and its wrong version is readable.
    std::string dll_path = WriteTestDll(TEST_DLL_BAD_ABI, "_badabi");
    if (dll_path.empty())
    {
        FAIL("Failed to build test plugin (g++ not on PATH?)");
        return;
    }

    DllHandle dll = LoadDll(dll_path.c_str());
    REQUIRE(dll.handle != nullptr);
    auto abi = reinterpret_cast<uint32_t (*)()>(
        GetDllSymbol(dll, "GetGameLogicAbiVersion"));
    REQUIRE(abi != nullptr);
    CHECK(abi() != RAYWAVES_GAMELOGIC_ABI_VERSION); // mismatch detectable
    UnloadDll(dll);
}

TEST_CASE("DllLoader: CleanupStaleShadowCopies removes only old shadow libs")
{
    fs::path shadow_dir = fs::current_path() / ".raywaves" / "shadows";
    fs::create_directories(shadow_dir);

    fs::path stale   = shadow_dir / "GameLogic.shadow.123.456.so";
    fs::path fresh   = shadow_dir / "GameLogic.shadow.123.999.so";
    fs::path foreign = shadow_dir / "unrelated.so";

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

    // Backdate 'stale' by 2 hours
    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    fs::last_write_time(stale, old_time);

    CleanupStaleShadowCopies();

    CHECK(!fs::exists(stale));  // old shadow copy removed
    CHECK(fs::exists(fresh));   // recent shadow copy kept
    CHECK(fs::exists(foreign)); // non-shadow file untouched

    fs::remove(fresh);
    fs::remove(foreign);
}

TEST_CASE(
    "DllLoader: log sink captures diagnostics, reset restores stderr default")
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
    fs::path stale = shadow_dir / "GameLogic.shadow.9.9.so";
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

    // Reset to the stderr default; must not crash and must accept new sinks
    // after
    SetDllLogSink(nullptr);
    SetDllLogSink(
        [&](std::string_view message, bool is_error)
        {
            (void)message;
            (void)is_error;
        });
    SetDllLogSink(nullptr);
}

TEST_CASE(
    "DllLoader: FormatAbiMismatchMessage distinguishes missing vs mismatched")
{
    std::string missing =
        FormatAbiMismatchMessage(false, 0, RAYWAVES_GAMELOGIC_ABI_VERSION);
    CHECK(missing.find("missing GetGameLogicAbiVersion") != std::string::npos);
    CHECK(missing.find("Rebuild GameLogic") != std::string::npos);

    std::string mismatch =
        FormatAbiMismatchMessage(true, 0xDEAD, RAYWAVES_GAMELOGIC_ABI_VERSION);
    CHECK(mismatch.find("ABI mismatch") != std::string::npos);
    CHECK(mismatch.find("57005") !=
          std::string::npos); // 0xDEAD reported DLL version
    CHECK(mismatch.find(std::to_string(RAYWAVES_GAMELOGIC_ABI_VERSION)) !=
          std::string::npos);
    CHECK(mismatch.find("Rebuild GameLogic") != std::string::npos);
}
