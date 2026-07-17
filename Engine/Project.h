#pragma once
#include <string>

struct t_Project 
{
    std::string m_RootPath;
    std::string m_Name;
    std::string m_Version;
    std::string m_EngineVersion;
    std::string m_SourceDir = "GameLogic";
    std::string m_AssetDir = "Assets";
    std::string m_EntryDll = "GameLogic.dll";
    std::string m_IconPath = "";
    
    std::string m_SourcePath;
    std::string m_AssetPath;
    std::string m_DllPath;

    // Editor state
    float m_CameraX = 0.f;
    float m_CameraY = 0.f;
    std::string m_LastMapId;
    int m_SceneWidth = 1280;
    int m_SceneHeight = 720;
    int m_TargetFPS = 60;

    bool m_bLoadFromFile(std::string_view manifest_path);
    bool m_bSaveToFile() const;
    bool m_bIsValid() const;
    
    static bool b_IsProjectFolder(std::string_view folder);
};
