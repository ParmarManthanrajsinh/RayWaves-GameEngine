#pragma once
#include <unordered_map>
#include <string>
#include <string_view>
#include <raylib.h>

// Transparent hash + equal enables string_view lookup without std::string allocation
struct TransparentHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
    size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
};
struct TransparentEqual {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const { return a == b; }
};

// Note: StateBag is allocated in the editor (main.exe) and passed by reference 
// into code running inside GameLogic.dll. Since std::string and std::unordered_map 
// allocate memory, this relies on both main.exe and GameLogic.dll linking the 
// same CRT (C Runtime). Under MSVC's default dynamic /MD runtime, this is safe.
// If either target is later switched to static CRT linkage (/MT), this may cause 
// heap-corruption crashes across the DLL boundary.

class StateBag {
public:
    void SetFloat(std::string_view key, float value) { m_Floats[std::string(key)] = value; }
    float GetFloat(std::string_view key, float defaultValue = 0.0f) const {
        auto it = m_Floats.find(key);
        return (it != m_Floats.end()) ? it->second : defaultValue;
    }

    void SetInt(std::string_view key, int value) { m_Ints[std::string(key)] = value; }
    int GetInt(std::string_view key, int defaultValue = 0) const {
        auto it = m_Ints.find(key);
        return (it != m_Ints.end()) ? it->second : defaultValue;
    }

    void SetBool(std::string_view key, bool value) { m_Bools[std::string(key)] = value; }
    bool GetBool(std::string_view key, bool defaultValue = false) const {
        auto it = m_Bools.find(key);
        return (it != m_Bools.end()) ? it->second : defaultValue;
    }

    void SetString(std::string_view key, std::string_view value) { m_Strings[std::string(key)] = std::string(value); }
    std::string GetString(std::string_view key, std::string_view defaultValue = "") const {
        auto it = m_Strings.find(key);
        return (it != m_Strings.end()) ? it->second : std::string(defaultValue);
    }

    void SetVector2(std::string_view key, Vector2 value) {
        SetFloat(std::string(key) + "_x", value.x);
        SetFloat(std::string(key) + "_y", value.y);
    }
    Vector2 GetVector2(std::string_view key, Vector2 defaultValue = {0.0f, 0.0f}) const {
        return { GetFloat(std::string(key) + "_x", defaultValue.x), GetFloat(std::string(key) + "_y", defaultValue.y) };
    }

    void Clear() {
        m_Floats.clear();
        m_Ints.clear();
        m_Bools.clear();
        m_Strings.clear();
    }

private:
    std::unordered_map<std::string, float, TransparentHash, TransparentEqual> m_Floats;
    std::unordered_map<std::string, int, TransparentHash, TransparentEqual> m_Ints;
    std::unordered_map<std::string, bool, TransparentHash, TransparentEqual> m_Bools;
    std::unordered_map<std::string, std::string, TransparentHash, TransparentEqual> m_Strings;
};
