#include "Project.h"
#include "EngineVersion.h"
#include "Log.h"
#include "PlatformInfo.h"
#include "PlatformNames.h"
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;

namespace
{
// Older manifests store "GameLogic.dll". Keep the stem and let the host
// platform choose the module suffix.
std::string NormalizeEntryModule(std::string value)
{
    fs::path entry(value);
    if (!entry.has_extension())
    {
        return value.empty() ? std::string("GameLogic") : value;
    }
    entry.replace_extension();
    return entry.empty() ? std::string("GameLogic") : entry.string();
}

// Malformed values keep the caller's previous value instead of throwing the
// whole manifest away.
int ParseIntOr(const std::string& text, int fallback)
{
    try
    {
        return std::stoi(text);
    }
    catch (...)
    {
        return fallback;
    }
}

// Manifests are written as 1/0; true/false is accepted for hand edits.
bool ParseBoolOr(const std::string& text, bool fallback)
{
    if (text == "1" || text == "true")
        return true;
    if (text == "0" || text == "false")
        return false;
    return fallback;
}
} // namespace

bool t_Project::m_bLoadFromFile(std::string_view manifest_path)
{
    std::string path_str(manifest_path);
    std::ifstream file(path_str);
    if (!file.is_open())
    {
        Log::Error("Project", "Project manifest not found: " + path_str);
        return false;
    }

    // Store root path relative to manifest
    m_RootPath = fs::path(manifest_path).parent_path().string();

    // Set default values before parsing
    m_Name = "NewProject";
    m_Version = "1.0.0";
    m_EngineVersion = RAYWAVES_VERSION;
    m_SourceDir = "GameLogic";
    m_AssetDir = "Assets";
    m_EntryDll = "GameLogic";
    m_IconPath = "";

    m_CameraX = 0.f;
    m_CameraY = 0.f;
    m_LastMapId = "";
    m_SceneWidth = 1280;
    m_SceneHeight = 720;
    m_TargetFPS = 60;

    m_ExportName = "";
    m_ExportPath = "export";
    m_ExportWidth = 1280;
    m_ExportHeight = 720;
    m_ExportFPS = 60;
    m_bExportFullscreen = false;
    m_bExportResizable = true;
    m_bExportVSync = true;

    std::string line;
    std::string currentSection;

    while (std::getline(file, line))
    {
        // Strip trailing \r for files with Windows CRLF line endings
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        // Trim leading whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty())
            continue;

        // Check for section headers
        if (line[0] == '[' && line.back() == ']')
        {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        // Find the equals sign
        auto equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
        {
            continue;
        }

        std::string key(line.begin(), line.begin() + static_cast<std::string::difference_type>(equal_pos));
        std::string value(line.begin() + static_cast<std::string::difference_type>(equal_pos) + 1, line.end());

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (currentSection == "project")
        {
            if (key == "name")
                m_Name = value;
            else if (key == "version")
                m_Version = value;
            else if (key == "engineVersion")
                m_EngineVersion = value;
            else if (key == "sourceDir")
                m_SourceDir = value;
            else if (key == "assetDir")
                m_AssetDir = value;
            else if (key == "entryDll")
                m_EntryDll = NormalizeEntryModule(value);
            else if (key == "iconPath")
                m_IconPath = value;
        }
        else if (currentSection == "editor")
        {
            auto safe_stof = [&](const std::string& strVal, float fallback) -> float
            {
                try
                {
                    return std::stof(strVal);
                }
                catch (...)
                {
                    return fallback;
                }
            };
            auto safe_stoi = [&](const std::string& strVal, int fallback) -> int
            {
                try
                {
                    return std::stoi(strVal);
                }
                catch (...)
                {
                    return fallback;
                }
            };
            if (key == "cameraX")
                m_CameraX = safe_stof(value, m_CameraX);
            else if (key == "cameraY")
                m_CameraY = safe_stof(value, m_CameraY);
            else if (key == "lastMapId")
                m_LastMapId = value;
            else if (key == "sceneWidth")
                m_SceneWidth = safe_stoi(value, m_SceneWidth);
            else if (key == "sceneHeight")
                m_SceneHeight = safe_stoi(value, m_SceneHeight);
            else if (key == "targetFPS")
                m_TargetFPS = safe_stoi(value, m_TargetFPS);
        }
        else if (currentSection == "export")
        {
            if (key == "gameName")
                m_ExportName = value;
            else if (key == "path")
                m_ExportPath = value;
            else if (key == "width")
                m_ExportWidth = ParseIntOr(value, m_ExportWidth);
            else if (key == "height")
                m_ExportHeight = ParseIntOr(value, m_ExportHeight);
            else if (key == "targetFPS")
                m_ExportFPS = ParseIntOr(value, m_ExportFPS);
            else if (key == "fullscreen")
                m_bExportFullscreen = ParseBoolOr(value, m_bExportFullscreen);
            else if (key == "resizable")
                m_bExportResizable = ParseBoolOr(value, m_bExportResizable);
            else if (key == "vsync")
                m_bExportVSync = ParseBoolOr(value, m_bExportVSync);
        }
    }

    file.close();

    // An explicitly blank path still means the default drop folder.
    if (m_ExportPath.empty())
        m_ExportPath = "export";

    // Resolve absolute paths. The manifest stores a logical stem; append the
    // host module suffix so Windows resolves .dll and Linux resolves .so.
    m_SourcePath = (fs::path(m_RootPath) / m_SourceDir).string();
    m_AssetPath = (fs::path(m_RootPath) / m_AssetDir).string();

    fs::path module_path(m_EntryDll);
    module_path += rw::platform::c_SharedLibrarySuffix;
    m_DllPath = (fs::path(m_RootPath) / module_path).string();

    Log::Debug("Project", "Loaded project: " + m_Name);
    return true;
}

bool t_Project::m_bSaveToFile() const
{
    const fs::path manifest_path = fs::path(m_RootPath) / "project.raywaves";

    std::ostringstream text_stream;
    text_stream << "[project]\n";
    text_stream << "name=" << m_Name << "\n";
    text_stream << "version=" << m_Version << "\n";
    text_stream << "engineVersion=" << m_EngineVersion << "\n";
    text_stream << "sourceDir=" << m_SourceDir << "\n";
    text_stream << "assetDir=" << m_AssetDir << "\n";
    text_stream << "entryDll=" << m_EntryDll << "\n";
    if (!m_IconPath.empty())
        text_stream << "iconPath=" << m_IconPath << "\n";
    text_stream << "\n";

    text_stream << "[editor]\n";
    text_stream << "cameraX=" << m_CameraX << "\n";
    text_stream << "cameraY=" << m_CameraY << "\n";
    text_stream << "lastMapId=" << m_LastMapId << "\n";
    text_stream << "sceneWidth=" << m_SceneWidth << "\n";
    text_stream << "sceneHeight=" << m_SceneHeight << "\n";
    text_stream << "targetFPS=" << m_TargetFPS << "\n";
    text_stream << "\n";

    text_stream << "[export]\n";
    text_stream << "gameName=" << m_ExportName << "\n";
    text_stream << "path=" << m_ExportPath << "\n";
    text_stream << "width=" << m_ExportWidth << "\n";
    text_stream << "height=" << m_ExportHeight << "\n";
    text_stream << "targetFPS=" << m_ExportFPS << "\n";
    text_stream << "fullscreen=" << (m_bExportFullscreen ? 1 : 0) << "\n";
    text_stream << "resizable=" << (m_bExportResizable ? 1 : 0) << "\n";
    text_stream << "vsync=" << (m_bExportVSync ? 1 : 0) << "\n";

    const std::string text = text_stream.str();

    // Snapshot the previous manifest before the atomic swap. A missing manifest
    // (first save) needs no backup.
    std::error_code exists_error;
    if (fs::exists(manifest_path, exists_error))
    {
        std::string backup_error;
        if (!PlatformInfo::b_BackupFile(manifest_path, backup_error))
        {
            Log::Error("Project", "Failed to back up project manifest " + manifest_path.string() + ": " + backup_error);
            return false;
        }
    }

    std::string write_error;
    if (!PlatformInfo::b_WriteFileAtomic(manifest_path, text, write_error))
    {
        Log::Error("Project", "Failed to save project manifest " + manifest_path.string() + ": " + write_error);
        return false;
    }

    Log::Debug("Project", "Saved project manifest to: " + manifest_path.string());
    return true;
}

bool t_Project::m_bIsValid() const
{
    return fs::exists(m_SourcePath) && fs::exists(m_AssetPath);
}

bool t_Project::b_IsProjectFolder(std::string_view folder)
{
    return fs::exists(fs::path(folder) / "project.raywaves");
}
