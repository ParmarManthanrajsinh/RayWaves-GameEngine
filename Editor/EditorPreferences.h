#pragma once
#include <string>

struct t_EditorPreferences
{
    float GuiScale = 1.0f;
    std::string ThemeName = "Charcoal";
    std::string FontFamily = "Roboto";
    bool AutoSaveEnabled = true;
    int AutoSaveIntervalSeconds = 60;

    // Scene viewport grid and snapping. The viewport reads these every frame,
    // so they are stored clamped rather than validated at the point of use.
    bool SceneGridEnabled = true;
    float SceneGridSize = 32.0f;
    bool SceneSnapEnabled = true;
    float SceneSnapStep = 1.0f;
    float SceneAngleSnapDegrees = 15.0f;
};

class EditorPreferences
{
public:
    static EditorPreferences& GetInstance();

    bool m_bLoadFromFile();
    bool m_bSaveToFile() const;

    t_EditorPreferences& GetPreferences() { return m_Preferences; }
    const t_EditorPreferences& GetPreferences() const { return m_Preferences; }

    static std::string GetConfigPath();

private:
    EditorPreferences() = default;
    t_EditorPreferences m_Preferences;
};
