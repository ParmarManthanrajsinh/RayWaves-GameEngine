#include "FileAssociation.h"
#include "../Engine/Platform/PlatformDesktop.h"
#include "../Engine/Platform/PlatformPaths.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kDesktopFileName = "raywaves-project.desktop";

    fs::path DataHomeDir()
    {
        const char* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg != nullptr && *xdg != '\0')
        {
            return fs::path(xdg);
        }
        const char* home = std::getenv("HOME");
        if (home != nullptr && *home != '\0')
        {
            return fs::path(home) / ".local" / "share";
        }
        return {};
    }

    fs::path ApplicationsDir()
    {
        fs::path data_home = DataHomeDir();
        if (data_home.empty())
        {
            return {};
        }
        return data_home / "applications";
    }

    fs::path MimePackagesDir()
    {
        fs::path data_home = DataHomeDir();
        if (data_home.empty())
        {
            return {};
        }
        return data_home / "mime" / "packages";
    }

    fs::path DesktopFilePath()
    {
        fs::path dir = ApplicationsDir();
        if (dir.empty())
        {
            return {};
        }
        return dir / kDesktopFileName;
    }

    // Desktop Entry spec: paths with spaces/specials go inside double quotes.
    std::string QuoteExecPath(const std::string& exe_path)
    {
        bool b_NeedsQuoting =
            exe_path.find_first_of(" \t\"\\$`<>|&;*?") != std::string::npos;
        if (!b_NeedsQuoting)
        {
            return exe_path;
        }
        std::string quoted = "\"";
        for (char c : exe_path)
        {
            if (c == '"' || c == '\\')
            {
                quoted += '\\';
            }
            quoted += c;
        }
        quoted += '"';
        return quoted;
    }

    void RefreshDesktopCaches()
    {
        fs::path apps = ApplicationsDir();
        if (!apps.empty())
        {
            std::string cmd =
                std::string("update-desktop-database '") + apps.string() +
                "' 2>/dev/null || true";
            std::system(cmd.c_str());
        }

        fs::path data_home = DataHomeDir();
        if (!data_home.empty())
        {
            std::string cmd =
                std::string("update-mime-database '") +
                (data_home / "mime").string() + "' 2>/dev/null || true";
            std::system(cmd.c_str());
        }
    }
}

std::string GetExecutablePath()
{
    std::string exe = platform::HostExecutablePath().string();
    if (exe.empty())
    {
        return fs::current_path().string();
    }
    return fs::path(exe).lexically_normal().string();
}

bool RegisterRayWavesFileAssociation()
{
    fs::path apps = ApplicationsDir();
    fs::path mime_dir = MimePackagesDir();
    if (apps.empty() || mime_dir.empty())
    {
        return false;
    }

    std::error_code ec;
    fs::create_directories(apps, ec);
    fs::create_directories(mime_dir, ec);
    if (ec)
    {
        return false;
    }

    std::string exe_path = GetExecutablePath();

    {
        std::string entry = platform::MakeDesktopEntry(
            "RayWaves", QuoteExecPath(exe_path) + " %f",
            platform::kEditorIconName, platform::kRayWavesMimeType);
        std::ofstream desktop_file(apps / kDesktopFileName);
        if (!desktop_file.is_open())
        {
            return false;
        }
        desktop_file << entry;
    }

    {
        std::string xml = platform::MakeMimeTypeXml(
            platform::kRayWavesMimeType, "*.raywaves");
        std::ofstream mime_xml(mime_dir / "raywaves-project.xml");
        if (!mime_xml.is_open())
        {
            return false;
        }
        mime_xml << xml;
    }

    RefreshDesktopCaches();
    return true;
}

bool IsRayWavesFileAssociationRegistered()
{
    fs::path desktop_file = DesktopFilePath();
    if (desktop_file.empty() || !fs::exists(desktop_file))
    {
        return false;
    }

    std::ifstream file(desktop_file);
    if (!file.is_open())
    {
        return false;
    }

    // Registered iff the desktop entry launches this very executable.
    std::string expected = "Exec=" + QuoteExecPath(GetExecutablePath());
    std::string line;
    while (std::getline(file, line))
    {
        if (line.compare(0, expected.size(), expected) == 0)
        {
            return true;
        }
    }
    return false;
}
