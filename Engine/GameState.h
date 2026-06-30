#pragma once
#include <unordered_map>
#include <string>
#include <raylib.h>

// Note: StateBag is allocated in the editor (main.exe) and passed by reference 
// into code running inside GameLogic.dll. Since std::string and std::unordered_map 
// allocate memory, this relies on both main.exe and GameLogic.dll linking the 
// same CRT (C Runtime). Under MSVC's default dynamic /MD runtime, this is safe.
// If either target is later switched to static CRT linkage (/MT), this may cause 
// heap-corruption crashes across the DLL boundary.

class StateBag {
public:
    void SetFloat(const std::string& key, float value) { m_Floats[key] = value; }
    float GetFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = m_Floats.find(key);
        return (it != m_Floats.end()) ? it->second : defaultValue;
    }

    void SetInt(const std::string& key, int value) { m_Ints[key] = value; }
    int GetInt(const std::string& key, int defaultValue = 0) const {
        auto it = m_Ints.find(key);
        return (it != m_Ints.end()) ? it->second : defaultValue;
    }

    void SetBool(const std::string& key, bool value) { m_Bools[key] = value; }
    bool GetBool(const std::string& key, bool defaultValue = false) const {
        auto it = m_Bools.find(key);
        return (it != m_Bools.end()) ? it->second : defaultValue;
    }

    void SetString(const std::string& key, const std::string& value) { m_Strings[key] = value; }
    std::string GetString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = m_Strings.find(key);
        return (it != m_Strings.end()) ? it->second : defaultValue;
    }

    void SetVector2(const std::string& key, Vector2 value) {
        SetFloat(key + "_x", value.x);
        SetFloat(key + "_y", value.y);
    }
    Vector2 GetVector2(const std::string& key, Vector2 defaultValue = {0.0f, 0.0f}) const {
        return { GetFloat(key + "_x", defaultValue.x), GetFloat(key + "_y", defaultValue.y) };
    }

private:
    std::unordered_map<std::string, float> m_Floats;
    std::unordered_map<std::string, int> m_Ints;
    std::unordered_map<std::string, bool> m_Bools;
    std::unordered_map<std::string, std::string> m_Strings;
};
