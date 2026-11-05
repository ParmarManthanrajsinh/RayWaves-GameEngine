#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

// Unique, self-cleaning directory for tests that touch the filesystem.
// Avoids collisions when suites run in parallel and leaves nothing behind.
class TempDir
{
public:
    explicit TempDir(std::string_view prefix = "raywaves_test")
    {
        static std::atomic<unsigned long long> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_Path = std::filesystem::temp_directory_path()
            / (std::string(prefix) + "_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(m_Path);
    }

    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(m_Path, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& Path() const { return m_Path; }
    std::string String() const { return m_Path.string(); }
    operator std::filesystem::path() const { return m_Path; }

private:
    std::filesystem::path m_Path;
};
