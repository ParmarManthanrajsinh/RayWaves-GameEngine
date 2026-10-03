#pragma once

#include <string>
#include <string_view>

// Single source for the XDG desktop entry and shared-mime-info XML that
// register a ".raywaves"-capable launcher. The editor's "Register .raywaves
// file association" command and every exported game's generated install.sh
// both build their files from these helpers, so the two cannot drift.
namespace platform
{
    // MIME type for RayWaves project files.
    inline constexpr std::string_view kRayWavesMimeType =
        "application/x-raywaves-project";

    // Icon file name (under hicolor/256x256/apps/) for the editor itself.
    inline constexpr std::string_view kEditorIconName = "raywaves";

    // Desktop Entry file body. `exec` is the raw value after "Exec=" - the
    // caller decides quoting (see QuoteExecPath in FileAssociation.cpp) and
    // which path goes there (live editor path, or the @EXEC@ placeholder an
    // exported game's install.sh substitutes at install time). Empty
    // `mime_type` omits the MimeType line: an exported game must not claim
    // the .raywaves handler away from the editor.
    inline std::string MakeDesktopEntry(std::string_view name,
                                        std::string_view exec,
                                        std::string_view icon_name,
                                        std::string_view mime_type)
    {
        std::string out;
        out.reserve(256);
        out += "[Desktop Entry]\n";
        out += "Type=Application\n";
        out += "Name=";
        out += name;
        out += "\n";
        out += "Comment=";
        out += name;
        out += " Project\n";
        out += "Exec=";
        out += exec;
        out += "\n";
        out += "Icon=";
        out += icon_name;
        out += "\n";
        if (!mime_type.empty())
        {
            out += "MimeType=";
            out += mime_type;
            out += ";\n";
        }
        out += "Terminal=false\n";
        out += "Categories=Development;IDE;\n";
        out += "StartupNotify=true\n";
        return out;
    }

    // shared-mime-info XML binding a file glob to the MIME type, so double
    // clicking a project file resolves on desktops that honor the database.
    inline std::string MakeMimeTypeXml(std::string_view mime_type,
                                       std::string_view glob_pattern)
    {
        std::string out;
        out.reserve(320);
        out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        out +=
            "<mime-info xmlns=\"http://www.freedesktop.org/standards/"
            "shared-mime-info\">\n";
        out += "  <mime-type type=\"";
        out += mime_type;
        out += "\">\n";
        out += "    <comment>RayWaves Project</comment>\n";
        out += "    <glob pattern=\"";
        out += glob_pattern;
        out += "\"/>\n";
        out += "  </mime-type>\n";
        out += "</mime-info>\n";
        return out;
    }

    // Data-home relative destination of an installed icon:
    // <icon_name>.png under hicolor/256x256/apps/.
    inline std::string InstalledIconRelPath(std::string_view icon_name)
    {
        return std::string("icons/hicolor/256x256/apps/") +
               std::string(icon_name) + ".png";
    }
}
