#include "PlatformInfo.h"
#include "doctest/doctest.h"
#include "support/TempDir.h"
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
std::string ReadAll(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& path, std::string_view contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

// A failed atomic write must not leave debris beside the target.
std::size_t CountTemporaryFiles(const fs::path& directory)
{
    std::size_t count = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory))
    {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos)
        {
            ++count;
        }
    }
    return count;
}
} // namespace

TEST_CASE("atomic write creates a new file")
{
    TempDir temp("raywaves_atomic_new");
    const fs::path target = temp.Path() / "notes.txt";
    std::string error = "stale";

    REQUIRE(PlatformInfo::b_WriteFileAtomic(target, "hello world", error));
    CHECK(error.empty());
    CHECK(ReadAll(target) == "hello world");
    CHECK(CountTemporaryFiles(temp.Path()) == 0);

    REQUIRE(PlatformInfo::b_WriteFileAtomic(temp.Path() / "empty.txt", "", error));
    CHECK(error.empty());
    CHECK(fs::exists(temp.Path() / "empty.txt"));
    CHECK(fs::file_size(temp.Path() / "empty.txt") == 0);
}

TEST_CASE("atomic write replaces existing contents")
{
    TempDir temp("raywaves_atomic_replace");
    const fs::path target = temp.Path() / "manifest.txt";
    WriteAll(target, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    std::string error;
    REQUIRE(PlatformInfo::b_WriteFileAtomic(target, "bb", error));
    CHECK(error.empty());
    CHECK(ReadAll(target) == "bb");
    CHECK(fs::file_size(target) == 2);
    CHECK(CountTemporaryFiles(temp.Path()) == 0);
}

TEST_CASE("atomic write fails when the parent directory is missing")
{
    TempDir temp("raywaves_atomic_noparent");
    const fs::path target = temp.Path() / "missing" / "notes.txt";
    std::string error;

    CHECK_FALSE(PlatformInfo::b_WriteFileAtomic(target, "contents", error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(fs::exists(temp.Path() / "missing"));
    CHECK_FALSE(fs::exists(target));
}

TEST_CASE("atomic write to a directory leaves the directory untouched")
{
    TempDir temp("raywaves_atomic_dir");
    const fs::path target = temp.Path() / "data.json";
    fs::create_directories(target);
    WriteAll(target / "keep.txt", "old contents");
    std::string error;

    CHECK_FALSE(PlatformInfo::b_WriteFileAtomic(target, "new contents", error));
    CHECK_FALSE(error.empty());
    CHECK(fs::is_directory(target));
    CHECK(ReadAll(target / "keep.txt") == "old contents");
    CHECK(CountTemporaryFiles(temp.Path()) == 0);
}

TEST_CASE("backup copies an existing file")
{
    TempDir temp("raywaves_backup");
    const fs::path source = temp.Path() / "project.ini";
    WriteAll(source, "version=1");
    std::string error = "stale";

    REQUIRE(PlatformInfo::b_BackupFile(source, error));
    CHECK(error.empty());
    CHECK(ReadAll(source) == "version=1");

    fs::path backup = source;
    backup += ".bak";
    REQUIRE(fs::exists(backup));
    CHECK(ReadAll(backup) == "version=1");

    // A second backup overwrites the previous snapshot.
    WriteAll(source, "version=2");
    REQUIRE(PlatformInfo::b_BackupFile(source, error));
    CHECK(error.empty());
    CHECK(ReadAll(backup) == "version=2");
}

TEST_CASE("backup fails for a missing source")
{
    TempDir temp("raywaves_backup_missing");
    const fs::path source = temp.Path() / "absent.ini";
    std::string error;

    CHECK_FALSE(PlatformInfo::b_BackupFile(source, error));
    CHECK_FALSE(error.empty());

    fs::path backup = source;
    backup += ".bak";
    CHECK_FALSE(fs::exists(backup));
}
