#include "FileAssociation.h"
#include "PlatformInfo.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::filesystem::path DataHome()
{
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    if (xdg_data != nullptr && xdg_data[0] != '\0')
    {
        return std::filesystem::path(xdg_data);
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0')
    {
        return std::filesystem::path(home) / ".local" / "share";
    }
    return std::filesystem::temp_directory_path();
}

std::filesystem::path DesktopFilePath()
{
    return DataHome() / "applications" / "raywaves.desktop";
}

std::string DesktopExecQuote(const std::string& value)
{
    std::string quoted = "\"";
    for (const char character : value)
    {
        if (character == '\\' || character == '"' || character == '`' || character == '$')
        {
            quoted += '\\';
        }
        quoted += character;
    }
    quoted += '"';
    return quoted;
}

bool WriteFile(const std::filesystem::path& path, const std::string& contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }
    file << contents;
    return file.good();
}

void RunQuiet(const char* command)
{
    const int result = std::system(command);
    (void)result;
}

std::string ShellQuotePath(const std::filesystem::path& path)
{
    std::string quoted = "'";
    for (const char character : path.string())
    {
        if (character == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted += character;
        }
    }
    quoted += '\'';
    return quoted;
}
} // namespace

std::string GetExecutablePath()
{
    return PlatformInfo::GetExecutablePath().lexically_normal().string();
}

bool RegisterRayWavesFileAssociation()
{
    const std::string exe_path = GetExecutablePath();
    if (exe_path.empty())
    {
        return false;
    }

    std::string desktop;
    desktop += "[Desktop Entry]\n";
    desktop += "Type=Application\n";
    desktop += "Name=RayWaves\n";
    desktop += "Comment=RayWaves game project\n";
    desktop += "Exec=" + DesktopExecQuote(exe_path) + " %f\n";
    desktop += "MimeType=application/x-raywaves-project;\n";
    desktop += "Terminal=false\n";
    desktop += "Categories=Development;\n";

    if (!WriteFile(DesktopFilePath(), desktop))
    {
        return false;
    }

    const std::filesystem::path mime_package = DataHome() / "mime" / "packages" / "raywaves-project.xml";
    std::string mime_xml;
    mime_xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    mime_xml += "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n";
    mime_xml += "  <mime-type type=\"application/x-raywaves-project\">\n";
    mime_xml += "    <comment>RayWaves Project</comment>\n";
    mime_xml += "    <glob pattern=\"*.raywaves\"/>\n";
    mime_xml += "  </mime-type>\n";
    mime_xml += "</mime-info>\n";
    WriteFile(mime_package, mime_xml);

    const std::filesystem::path data_home = DataHome();
    const std::string mime_command = "update-mime-database " + ShellQuotePath(data_home / "mime") + " >/dev/null 2>&1";
    const std::string desktop_command =
        "update-desktop-database " + ShellQuotePath(data_home / "applications") + " >/dev/null 2>&1";
    RunQuiet(mime_command.c_str());
    RunQuiet(desktop_command.c_str());
    RunQuiet("xdg-mime default raywaves.desktop application/x-raywaves-project >/dev/null 2>&1");
    return true;
}

bool IsRayWavesFileAssociationRegistered()
{
    const std::filesystem::path desktop_path = DesktopFilePath();
    std::ifstream file(desktop_path);
    if (!file.is_open())
    {
        return false;
    }

    const std::string current = GetExecutablePath();
    std::string line;
    bool b_has_exec = false;
    bool b_has_mime = false;
    while (std::getline(file, line))
    {
        if (line.rfind("Exec=", 0) == 0)
        {
            b_has_exec = line.find(current) != std::string::npos;
        }
        else if (line.rfind("MimeType=", 0) == 0)
        {
            b_has_mime = line.find("application/x-raywaves-project") != std::string::npos;
        }
    }
    return b_has_exec && b_has_mime;
}
