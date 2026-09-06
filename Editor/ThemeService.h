#pragma once

#include <string>
#include <string_view>

// Theme rebaking for the editor, extracted from GameEditor so the frame loop
// stays orchestration-only. All state lives in EditorPreferences; this class
// is stateless. Call RebakeNow() from Init (first paint) and whenever the
// m_bNeedsThemeRebake flag is set (e.g. after changing theme preferences).
class ThemeService
{
public:
    static void RebakeNow();

    // EngineContent-relative asset lookup with a Core/EngineContent fallback
    // (used for fonts and the window icon).
    static std::string GetEngineContentPath(std::string_view sub_path);
};
