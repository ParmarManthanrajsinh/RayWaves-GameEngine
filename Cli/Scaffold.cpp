#include "Scaffold.h"
#include "CliCommon.h"
#include "Json.h"
#include "ProjectManager.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace
{
bool b_IsIdentifier(std::string_view name)
{
    if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name.front())) && name.front() != '_'))
    {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(),
        [](char character) { return std::isalnum(static_cast<unsigned char>(character)) || character == '_'; });
}

// Map ids become AssetResolver keys and scene file stems, so reject anything
// that would need escaping in a C++ string literal or a path.
bool b_IsSafeMapId(std::string_view map_id)
{
    if (map_id.empty())
    {
        return false;
    }
    return std::all_of(map_id.begin(), map_id.end(),
        [](char character)
        {
            const unsigned char value = static_cast<unsigned char>(character);
            return std::isalnum(value) || character == '_' || character == '-' || character == '.' || character == ' ';
        });
}

std::string LineEnding(const std::string& text)
{
    return (text.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
}

size_t LineStart(const std::string& text, size_t position)
{
    if (position > text.size())
    {
        position = text.size();
    }
    const size_t newline = text.rfind('\n', position);
    return (newline == std::string::npos) ? 0 : newline + 1;
}

std::string IndentAt(const std::string& text, size_t position)
{
    const size_t start = LineStart(text, position);
    size_t cursor = start;
    while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t'))
    {
        ++cursor;
    }
    return text.substr(start, cursor - start);
}

std::string ReadTextFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string InsertInclude(std::string text, std::string_view header)
{
    const std::string include = "#include \"" + std::string(header) + "\"";
    if (text.find(include) != std::string::npos)
    {
        return text;
    }

    const std::string eol = LineEnding(text);
    const size_t last_include = text.rfind("#include");
    if (last_include == std::string::npos)
    {
        return include + eol + text;
    }

    const size_t line_end = text.find('\n', last_include);
    if (line_end == std::string::npos)
    {
        return text + eol + include + eol;
    }
    return text.insert(line_end + 1, include + eol);
}

// Inserts AddLevel<Type>("id"); immediately before the Start(...) line.
bool b_InsertAddLevel(std::string& text, std::string_view type_name, std::string_view map_id, std::string& out_warning)
{
    if (text.find("AddLevel<" + std::string(type_name)) != std::string::npos)
    {
        return false;
    }

    const size_t start = text.find("Start(");
    if (start == std::string::npos)
    {
        out_warning = "Could not find a Start(...) line; add AddLevel<" + std::string(type_name) + ">(\"" +
            std::string(map_id) + "\"); manually.";
        return false;
    }

    const size_t line_start = LineStart(text, start);
    const std::string indent = IndentAt(text, start);
    const std::string eol = LineEnding(text);
    const std::string line =
        indent + "AddLevel<" + std::string(type_name) + ">(\"" + std::string(map_id) + "\");" + eol;

    text.insert(line_start, line);
    return true;
}

// Raw MapManager projects use RegisterMap<T>("id"); insert after the last one.
bool b_InsertRegisterMap(
    std::string& text, std::string_view type_name, std::string_view map_id, std::string& out_warning)
{
    if (text.find("RegisterMap<" + std::string(type_name)) != std::string::npos)
    {
        return false;
    }

    const size_t last = text.rfind("RegisterMap<");
    if (last == std::string::npos)
    {
        out_warning =
            "Could not find AddLevel<...> or RegisterMap<...>; register " + std::string(type_name) + " manually.";
        return false;
    }

    const size_t line_end = text.find('\n', last);
    const std::string indent = IndentAt(text, last);
    const std::string eol = LineEnding(text);
    const std::string line =
        indent + "RegisterMap<" + std::string(type_name) + ">(\"" + std::string(map_id) + "\");" + eol;

    if (line_end == std::string::npos)
    {
        text += eol + line;
    }
    else
    {
        text.insert(line_end + 1, line);
    }
    return true;
}

bool b_FindEntryPoint(const fs::path& source_dir, fs::path& out_path)
{
    std::error_code error;
    if (!fs::is_directory(source_dir, error))
    {
        return false;
    }

    for (const char* candidate : { "RootManager.cpp", "Game.cpp", "Main.cpp" })
    {
        const fs::path path = source_dir / candidate;
        if (fs::is_regular_file(path, error))
        {
            const std::string text = ReadTextFile(path);
            if (text.find("RW_GAME(") != std::string::npos)
            {
                out_path = path;
                return true;
            }
        }
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(source_dir, error))
    {
        if (!entry.is_regular_file(error) || entry.path().extension() != ".cpp")
        {
            continue;
        }
        const std::string text = ReadTextFile(entry.path());
        if (text.find("RW_GAME(") != std::string::npos)
        {
            out_path = entry.path();
            return true;
        }
    }

    return false;
}

std::string MakeLevelHeader(std::string_view name, std::string_view map_id)
{
    std::ostringstream out;
    out << "#pragma once\n"
        << "#include \"rw.h\"\n"
        << "\n"
        << "class " << name << " : public rw::Level\n"
        << "{\n"
        << "public:\n"
        << "    " << name << "() : rw::Level(\"" << map_id << "\") {}\n"
        << "\n"
        << "protected:\n"
        << "    void OnStart() override\n"
        << "    {\n"
        << "        // Spawn entities here. The editor registers them automatically.\n"
        << "    }\n"
        << "\n"
        << "    void OnUpdate(float delta_time) override { (void)delta_time; }\n"
        << "\n"
        << "    void OnDraw() override {}\n"
        << "\n"
        << "    void OnDrawUI() override { DrawText(\"" << name << "\", 20, 20, 20, LIGHTGRAY); }\n"
        << "};\n";
    return out.str();
}
} // namespace

namespace Cli
{
int TemplatesCommand(bool b_json)
{
    const std::vector<std::string> templates = ProjectManager::GetAvailableTemplates();
    if (b_json)
    {
        std::vector<std::string> items;
        items.reserve(templates.size());
        for (const std::string& name : templates)
        {
            items.push_back(CliJson::Quoted(name));
        }
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(true) << "," << CliJson::Key("templates")
                  << CliJson::Array(items) << "}\n";
        return 0;
    }

    for (const std::string& name : templates)
    {
        std::cout << name << "\n";
    }
    return 0;
}

int NewCommand(const FArgs& args, bool b_json)
{
    if (args.m_Positional.size() < 2)
    {
        PrintError(b_json, "usage", "rwcli new <targetDir> [--template Simple2D] [--json]");
        return 2;
    }

    const std::string target = args.m_Positional[1];
    const std::string template_name = args.Get("template", "Simple2D");

    const std::vector<std::string> templates = ProjectManager::GetAvailableTemplates();
    if (std::find(templates.begin(), templates.end(), template_name) == templates.end())
    {
        std::string available;
        for (const std::string& name : templates)
        {
            if (!available.empty())
            {
                available += ", ";
            }
            available += name;
        }
        PrintError(b_json, "unknown_template",
            "Unknown template '" + template_name + "'. Available: " + (available.empty() ? "none" : available));
        return 3;
    }

    std::string error;
    if (!ProjectManager::b_CreateProject(target, template_name, error))
    {
        PrintError(b_json, "create_failed", error);
        return 3;
    }

    std::error_code path_error;
    const fs::path absolute = fs::weakly_canonical(fs::absolute(fs::path(target), path_error), path_error);
    const std::string project_path = path_error ? fs::absolute(fs::path(target)).string() : absolute.string();

    if (b_json)
    {
        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(true) << "," << CliJson::Key("project")
                  << CliJson::Quoted(project_path) << "," << CliJson::Key("template") << CliJson::Quoted(template_name)
                  << "," << CliJson::Key("nextSteps")
                  << CliJson::Array(
                         { CliJson::Quoted("rwcli lsp " + project_path), CliJson::Quoted("rwcli build " + project_path),
                             CliJson::Quoted("rwcli run " + project_path + " --frames 60") })
                  << "}\n";
    }
    else
    {
        std::cout << "created " << project_path << " from " << template_name << "\n";
        std::cout << "next: rwcli lsp " << project_path << "\n";
        std::cout << "next: rwcli build " << project_path << "\n";
    }
    return 0;
}

int NewLevelCommand(const FArgs& args, bool b_json)
{
    if (args.m_Positional.size() < 3)
    {
        PrintError(b_json, "usage", "rwcli new-level <project> <Name> [--id MapId] [--dry-run] [--json]");
        return 2;
    }

    FProjectContext project;
    std::string error;
    if (!b_ResolveProject(args.m_Positional[1], project, error))
    {
        PrintError(b_json, "invalid_project", error);
        return 3;
    }

    const std::string name = args.m_Positional[2];
    if (!b_IsIdentifier(name))
    {
        PrintError(b_json, "invalid_name", "Level name must be a C++ identifier: " + name);
        return 2;
    }

    const std::string map_id = args.Get("id", name);
    if (!b_IsSafeMapId(map_id))
    {
        PrintError(b_json, "invalid_map_id", "Map id may contain letters, digits, spaces, '_', '-' and '.': " + map_id);
        return 2;
    }

    fs::path source_dir = project.m_Project.m_SourcePath;
    if (source_dir.empty())
    {
        source_dir = project.m_Dir / project.m_Project.m_SourceDir;
    }

    std::error_code fs_error;
    if (!fs::is_directory(source_dir, fs_error))
    {
        PrintError(b_json, "missing_source_dir", "Source folder not found: " + source_dir.string());
        return 3;
    }

    const bool b_dry_run = args.b_Has("dry-run");
    const fs::path header_path = source_dir / (name + ".h");
    const bool b_header_exists = fs::exists(header_path, fs_error);

    if (!b_header_exists && !b_dry_run)
    {
        std::ofstream header(header_path, std::ios::binary | std::ios::trunc);
        if (!header.is_open())
        {
            PrintError(b_json, "write_failed", "Could not write " + header_path.string());
            return 3;
        }
        header << MakeLevelHeader(name, map_id);
    }

    std::vector<std::string> warnings;
    std::string entry_point;
    bool b_include_added = false;
    bool b_registration_added = false;

    fs::path entry_path;
    if (b_FindEntryPoint(source_dir, entry_path))
    {
        entry_point = entry_path.string();
        std::string text = ReadTextFile(entry_path);
        if (text.empty())
        {
            warnings.push_back("Entry point is empty: " + entry_point);
        }
        else
        {
            const std::string before_include = text;
            text = InsertInclude(text, name + ".h");
            b_include_added = (text != before_include);

            std::string insertion_warning;
            if (text.find("AddLevel<") != std::string::npos)
            {
                b_registration_added = b_InsertAddLevel(text, name, map_id, insertion_warning);
            }
            else if (text.find("RegisterMap<") != std::string::npos)
            {
                b_registration_added = b_InsertRegisterMap(text, name, map_id, insertion_warning);
            }
            else
            {
                insertion_warning = "No AddLevel<...> or RegisterMap<...> found in " + entry_point + "; register " +
                    name + " manually.";
            }

            if (!insertion_warning.empty())
            {
                warnings.push_back(insertion_warning);
            }

            if (!b_dry_run && (b_include_added || b_registration_added))
            {
                std::ofstream entry_file(entry_path, std::ios::binary | std::ios::trunc);
                if (!entry_file.is_open())
                {
                    PrintError(b_json, "write_failed", "Could not write " + entry_path.string());
                    return 3;
                }
                entry_file << text;
            }
        }
    }
    else
    {
        warnings.push_back(
            "No .cpp file with RW_GAME(...) found in " + source_dir.string() + "; register " + name + " manually.");
    }

    if (b_json)
    {
        std::vector<std::string> warning_json;
        warning_json.reserve(warnings.size());
        for (const std::string& warning : warnings)
        {
            warning_json.push_back(CliJson::Quoted(warning));
        }

        std::cout << "{" << CliJson::Key("ok") << CliJson::Bool(true) << "," << CliJson::Key("level")
                  << CliJson::Quoted(name) << "," << CliJson::Key("mapId") << CliJson::Quoted(map_id) << ","
                  << CliJson::Key("header") << CliJson::Quoted(header_path.string()) << "," << CliJson::Key("created")
                  << CliJson::Bool(!b_header_exists) << "," << CliJson::Key("entryPoint")
                  << CliJson::Quoted(entry_point) << "," << CliJson::Key("includeAdded")
                  << CliJson::Bool(b_include_added) << "," << CliJson::Key("registrationAdded")
                  << CliJson::Bool(b_registration_added) << "," << CliJson::Key("dryRun") << CliJson::Bool(b_dry_run)
                  << "," << CliJson::Key("warnings") << CliJson::Array(warning_json) << "}\n";
    }
    else
    {
        if (!b_header_exists)
        {
            std::cout << (b_dry_run ? "would create " : "created ") << header_path.string() << "\n";
        }
        else
        {
            std::cout << "kept existing " << header_path.string() << "\n";
        }
        if (b_registration_added)
        {
            std::cout << (b_dry_run ? "would register " : "registered ") << name << " in " << entry_point << "\n";
        }
        for (const std::string& warning : warnings)
        {
            std::cerr << "warning: " << warning << "\n";
        }
    }

    return 0;
}
} // namespace Cli
