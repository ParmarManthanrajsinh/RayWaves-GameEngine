#pragma once
#include <string>

struct t_EditorPreferences
{
    float gui_scale = 1.0f;
    std::string theme_name = "Charcoal";
    std::string font_family = "Roboto";
};

class EditorPreferences
{
public:
    static EditorPreferences& GetInstance();
    
    bool m_bLoadFromFile();
    bool m_bSaveToFile() const;

    t_EditorPreferences& GetPreferences() { return m_Preferences; } 
    const t_EditorPreferences& GetPreferences() const { return m_Preferences; }
    
    static std::string GetConfigPath() ;

private:
    EditorPreferences() = default;
    t_EditorPreferences m_Preferences;
};
