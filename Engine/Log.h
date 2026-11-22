#pragma once
#include <functional>
#include <string_view>

namespace Log
{
enum class ELevel
{
    Debug,
    Info,
    Warn,
    Error,
};

using FSink = std::function<void(ELevel, std::string_view category, std::string_view message)>;

// An empty sink restores the default stderr sink, so tests and hosts can
// opt out without a separate reset entry point.
void SetSink(FSink sink);
void SetLevel(ELevel minimum);

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Debug(std::string_view category, std::string_view message);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Info(std::string_view category, std::string_view message);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Warn(std::string_view category, std::string_view message);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Error(std::string_view category, std::string_view message);
} // namespace Log
