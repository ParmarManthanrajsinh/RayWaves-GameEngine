#include "ProjectManager.h"
#include "EngineVersion.h"
#include "PlatformInfo.h"
#include "PlatformNames.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace
{
bool b_WriteProjectFiles(const fs::path& project_root, const t_Project& project, bool b_force);
}

fs::path ProjectManager::GetEngineRootDirectory()
{
    const fs::path base_dir = PlatformInfo::GetExecutablePath().parent_path();

    const auto b_has_engine_marker = [](const fs::path& dir)
    {
        return fs::exists(dir / "Engine") &&
            (fs::exists(dir / "Tools" / "toolchain.ini") || fs::exists(dir / "Tools" / "setup_tools.ps1") ||
                fs::exists(dir / "Tools" / "setup_tools.sh") || fs::exists(dir / "CMakeLists.txt"));
    };

    // Distribution layout: the editor sits next to Core/, which contains the
    // engine headers/sources. Check this before walking up so a dist folder
    // inside the dev tree does not resolve to the dev root.
    if (fs::exists(base_dir / "Core" / "Engine") &&
        (fs::exists(base_dir / "Core" / "Project.CMakeLists.in") || fs::exists(base_dir / "Core" / "raylib")))
    {
        return base_dir;
    }

    if (b_has_engine_marker(base_dir))
    {
        return base_dir;
    }

    fs::path current = base_dir;
    while (current.has_parent_path() && current != current.parent_path())
    {
        if (b_has_engine_marker(current))
        {
            return current;
        }
        current = current.parent_path();
    }

    // Fallback: check for dist layout (Core/ + templates/)
    if (fs::exists(base_dir / "Core") && fs::exists(base_dir / "templates"))
    {
        return base_dir;
    }

    current = base_dir;
    while (current.has_parent_path() && current != current.parent_path())
    {
        if (fs::exists(current / "Distribution" / "templates"))
        {
            return current;
        }
        current = current.parent_path();
    }

    return base_dir;
}

fs::path ProjectManager::GetToolsDirectory()
{
    fs::path engine_root = GetEngineRootDirectory();
    fs::path tools_dir = engine_root / "Core" / "Tools";
    const bool b_has_tools = fs::exists(tools_dir / "zig-cc.bat") || fs::exists(tools_dir / "setup_tools.sh") ||
        fs::exists(tools_dir / "setup_tools.ps1");
    if (!b_has_tools)
    {
        tools_dir = engine_root / "Tools";
    }
    return tools_dir;
}

std::string ProjectManager::SanitizeCMakeProjectName(std::string_view name)
{
    std::string sanitized;
    for (char c : name)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
        {
            sanitized += c;
        }
        else
        {
            sanitized += '_';
        }
    }

    if (!sanitized.empty() && sanitized[0] >= '0' && sanitized[0] <= '9')
    {
        sanitized = "_" + sanitized;
    }

    if (sanitized.empty())
    {
        sanitized = "RayWavesProject";
    }

    return sanitized;
}

t_Project ProjectManager::s_Current;
bool ProjectManager::s_bOpen = false;
std::string ProjectManager::s_RecentPath;
std::recursive_mutex ProjectManager::s_Mutex;

void ProjectManager::InitializeRecentPath()
{
    if (!s_RecentPath.empty())
        return;

    fs::path dir = PlatformInfo::GetAppConfigDirectory() / "RayWaves";
    std::error_code error;
    fs::create_directories(dir, error);
    s_RecentPath = (dir / "recent.ini").string();
}

bool ProjectManager::b_OpenProject(std::string_view folder_path, bool b_add_recent)
{
    std::scoped_lock lock(s_Mutex);
    std::string manifest_path = (fs::path(folder_path) / "project.raywaves").string();
    if (!fs::exists(manifest_path))
    {
        std::cerr << "Project manifest not found at: " << manifest_path << '\n';
        return false;
    }

    t_Project new_project;
    if (new_project.m_bLoadFromFile(manifest_path))
    {
        s_Current = new_project;
        s_bOpen = true;
        if (b_add_recent)
        {
            AddRecent(folder_path);
        }

        // Keep the agent guide self-contained inside the project. Never
        // overwrite a guide the project owner edited.
        b_WriteProjectFiles(fs::path(folder_path), s_Current, false);

        // Ensure project local cache directory exists
        fs::path raywaves_dir = fs::path(folder_path) / ".raywaves";
        fs::create_directories(raywaves_dir / "shadows");
        fs::create_directories(raywaves_dir / "build");

        // Assets and code folders are part of the project contract. Older
        // projects may be missing one; create it instead of rejecting them.
        fs::create_directories(s_Current.m_AssetPath);
        fs::create_directories(s_Current.m_SourcePath);

        return GenerateCMakeLists();
    }
    return false;
}

void ProjectManager::CloseProject()
{
    std::scoped_lock lock(s_Mutex);
    s_bOpen = false;
    s_Current = t_Project{};
}

bool ProjectManager::b_CreateProject(std::string_view target_folder, std::string_view template_name)
{
    fs::path target_path = target_folder;
    if (fs::exists(target_path))
    {
        std::cerr << "Target folder already exists: " << target_folder << '\n';
        return false;
    }

    // Resolve template path using robust root discovery
    fs::path root_dir = GetEngineRootDirectory();
    fs::path template_dir = root_dir / "templates" / template_name;

    if (!fs::exists(template_dir))
    {
        template_dir = root_dir / "Distribution" / "templates" / template_name;
    }

    if (!fs::exists(template_dir))
    {
        std::cerr << "Template not found: " << template_dir.string() << '\n';
        return false;
    }

    try
    {
        fs::copy(template_dir, target_path, fs::copy_options::recursive);

        // Rewrite the manifest name
        std::string manifest_path = (target_path / "project.raywaves").string();
        t_Project proj;
        if (proj.m_bLoadFromFile(manifest_path))
        {
            proj.m_Name = target_path.filename().string();
            proj.m_bSaveToFile();

            fs::create_directories(target_path / proj.m_AssetDir);
            fs::create_directories(target_path / proj.m_SourceDir);
            b_WriteProjectFiles(target_path, proj, false);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create project from template: " << e.what() << '\n';
        return false;
    }
}

bool ProjectManager::b_SaveCurrentProject()
{
    std::scoped_lock lock(s_Mutex);
    if (!s_bOpen)
        return false;
    return s_Current.m_bSaveToFile();
}

t_Project& ProjectManager::GetCurrent()
{
    std::scoped_lock lock(s_Mutex);
    return s_Current;
}

t_Project ProjectManager::GetCurrentCopy()
{
    std::scoped_lock lock(s_Mutex);
    return s_Current;
}

bool ProjectManager::b_HasOpenProject()
{
    std::scoped_lock lock(s_Mutex);
    return s_bOpen;
}

std::string ProjectManager::SetRecentPathForTesting(std::string_view path)
{
    std::scoped_lock lock(s_Mutex);
    std::string previous = s_RecentPath;
    s_RecentPath = std::string(path);
    return previous;
}

void ProjectManager::AddRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();

    // Normalize all paths so comparison works even for legacy entries
    // stored without normalization.
    for (auto& entry : recent)
    {
        entry = fs::path(entry).lexically_normal().string();
    }

    // Normalize path
    std::string norm_path = fs::path(path).lexically_normal().string();

    // Remove if already exists
    std::erase(recent, norm_path);

    // Insert at front
    recent.insert(recent.begin(), norm_path);

    // Keep only top 10
    if (recent.size() > 10)
        recent.resize(10);

    // Save
    std::ofstream file(s_RecentPath);
    if (file.is_open())
    {
        for (size_t i = 0; i < recent.size(); ++i)
        {
            file << "path" << i << "=" << recent[i] << "\n";
        }
    }
}

void ProjectManager::RemoveRecent(std::string_view path)
{
    InitializeRecentPath();
    std::vector<std::string> recent = GetRecent();

    // Normalize all paths so comparison works even for legacy entries
    // stored without normalization.
    for (auto& entry : recent)
    {
        entry = fs::path(entry).lexically_normal().string();
    }

    // Normalize path
    std::string norm_path = fs::path(path).lexically_normal().string();

    // Remove if exists
    std::erase(recent, norm_path);

    // Save
    std::ofstream file(s_RecentPath);
    if (file.is_open())
    {
        for (size_t i = 0; i < recent.size(); ++i)
        {
            file << "path" << i << "=" << recent[i] << "\n";
        }
    }
}

std::vector<std::string> ProjectManager::GetRecent()
{
    InitializeRecentPath();
    std::vector<std::string> recent;

    std::ifstream file(s_RecentPath);
    if (!file.is_open())
        return recent;

    std::string line;
    while (std::getline(file, line))
    {
        auto equal_pos = line.find('=');
        if (equal_pos != std::string::npos)
        {
            std::string value(line.begin() + equal_pos + 1, line.end());
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            // Keep missing entries: UI shows them greyed as "(missing)" and
            // lets the user remove them from the list.
            if (!value.empty())
            {
                recent.push_back(value);
            }
        }
    }
    return recent;
}

std::vector<std::string> ProjectManager::GetAvailableTemplates()
{
    std::vector<std::string> templates;
    fs::path root_dir = GetEngineRootDirectory();
    fs::path template_dir = root_dir / "templates";

    if (!fs::exists(template_dir))
    {
        template_dir = root_dir / "Distribution" / "templates";
    }

    if (fs::exists(template_dir) && fs::is_directory(template_dir))
    {
        for (const auto& entry : fs::directory_iterator(template_dir))
        {
            if (entry.is_directory())
            {
                templates.push_back(entry.path().filename().string());
            }
        }
    }

    // Directory order is filesystem-dependent; the New Project wizard and the
    // tests both expect a stable, alphabetical list.
    std::sort(templates.begin(), templates.end());
    return templates;
}

namespace
{
std::string ReplaceToken(std::string text, std::string_view token, std::string_view value)
{
    size_t position = 0;
    while ((position = text.find(token, position)) != std::string::npos)
    {
        text.replace(position, token.size(), value);
        position += value.size();
    }
    return text;
}

std::string ToForwardSlashes(fs::path path)
{
    std::string result = path.string();
    std::ranges::replace(result, '\\', '/');
    return result;
}

// The same template ships in dev builds (build/<preset>/packaging) and in
// distributions (Core/). Keeping one file avoids the two project build
// recipes drifting apart again.
fs::path FindProjectTemplate(const fs::path& exe_dir, const fs::path& engine_root)
{
    const fs::path candidates[] = {
        exe_dir / "packaging" / "Project.CMakeLists.in",
        exe_dir / "Core" / "Project.CMakeLists.in",
        engine_root / "packaging" / "Project.CMakeLists.in",
        engine_root / "Core" / "Project.CMakeLists.in",
    };

    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
            return candidate;
    }
    return {};
}

fs::path FindShippedFile(const fs::path& exe_dir, const fs::path& engine_root, std::string_view filename)
{
    const fs::path candidates[] = {
        exe_dir / "packaging" / filename,
        exe_dir / "Core" / filename,
        engine_root / "packaging" / filename,
        engine_root / "Core" / filename,
    };

    for (const auto& candidate : candidates)
    {
        if (fs::exists(candidate))
            return candidate;
    }
    return {};
}

std::string ReadTemplate(const fs::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool b_WriteText(const fs::path& path, std::string_view content, bool b_force)
{
    if (!b_force && fs::exists(path))
    {
        return true;
    }
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }
    file << content;
    return file.good();
}

// Keep generated files out of git without touching a project that has no
// .gitignore. Entries are appended once, after a marker comment.
void EnsureGitignoreEntries(const fs::path& project_root)
{
    const fs::path path = project_root / ".gitignore";
    std::error_code error;
    if (!fs::exists(path, error))
    {
        return;
    }

    std::string content = ReadTemplate(path);
    const char* entries[] = { "compile_commands.json", ".clangd", ".vscode/", ".cache/" };
    bool b_changed = false;
    for (const char* entry : entries)
    {
        if (content.find(entry) == std::string::npos)
        {
            if (!b_changed)
            {
                content += "\n# RayWaves generated editor files\n";
            }
            content += std::string(entry) + "\n";
            b_changed = true;
        }
    }

    if (b_changed)
    {
        std::ofstream file(path, std::ios::trunc);
        if (file.is_open())
        {
            file << content;
        }
    }
}

std::string JsonPath(const fs::path& path)
{
    std::string text = ToForwardSlashes(path);
    std::string quoted = "\"";
    for (const char character : text)
    {
        if (character == '"' || character == '\\')
        {
            quoted += '\\';
        }
        quoted += character;
    }
    quoted += '"';
    return quoted;
}

// Absolute paths for clangd/IntelliSense. Missing pieces are skipped so an
// engine checkout without a staged raylib still produces a usable file.
std::vector<fs::path> ZigLibcIncludeDirs(const fs::path& zig_root)
{
    std::vector<fs::path> dirs;
    const fs::path include_root = zig_root / "lib" / "libc" / "include";
    std::error_code error;
    if (!fs::is_directory(include_root, error))
    {
        return dirs;
    }

    // Include only the host target's headers. Adding every architecture's
    // directory makes clangd parse conflicting definitions (notably glibc's
    // __int32_t typedefs).
    std::vector<std::string> names;
    if (rw::platform::b_IsWindows)
    {
        names = { "any-windows-any" };
    }
    else
    {
#if defined(__aarch64__)
        const std::string arch = "aarch64";
#elif defined(__x86_64__)
        const std::string arch = "x86_64";
#elif defined(__i386__)
        const std::string arch = "x86";
#else
        const std::string arch;
#endif
        names = { "any-linux-any", "generic-glibc" };
        if (!arch.empty())
        {
            names.push_back(arch + "-linux-any");
            names.push_back(arch + "-linux-gnu");
        }
    }

    for (const std::string& name : names)
    {
        const fs::path candidate = include_root / name;
        if (fs::is_directory(candidate, error))
        {
            dirs.push_back(candidate);
        }
    }
    return dirs;
}

std::string BuildClangdContent(const fs::path& engine_dir, const fs::path& raylib_include, const fs::path& zig_root,
    const std::string& template_text)
{
    const fs::path zig_libcxx = zig_root / "lib" / "libcxx" / "include";
    const fs::path zig_libcxxabi = zig_root / "lib" / "libcxxabi" / "include";

    std::string target_flags;
    if (rw::platform::b_IsWindows && fs::exists(zig_libcxx))
    {
        target_flags += "    - -target\n";
        target_flags += "    - x86_64-windows-gnu\n";
        target_flags += "    - -nostdinc++\n";
        target_flags += "    - -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE\n";
        target_flags += "    - -D_LIBCPP_HAS_WIDE_CHARACTERS=1\n";
        target_flags += "    - -D_LIBCPP_ABI_VERSION=1\n";
        target_flags += "    - -D_LIBCPP_ABI_NAMESPACE=__1\n";
        target_flags += "    - -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS=1\n";
        target_flags += "    - -D_LIBCPP_HAS_NO_STD_MODULES=1\n";
        target_flags += "    - -D_LIBCPP_HAS_LOCALIZATION=1\n";
        target_flags += "    - -D_LIBCPP_HAS_FILESYSTEM=1\n";
        target_flags += "    - -D_LIBCPP_HAS_THREADS=1\n";
        target_flags += "    - -D_LIBCPP_HAS_RANDOM_DEVICE=1\n";
        target_flags += "    - -D_LIBCPP_HAS_TERMINAL=1\n";
        target_flags += "    - -D_LIBCPP_HAS_MONOTONIC_CLOCK=1\n";
        target_flags += "    - -D_LIBCPP_HAS_UNICODE=1\n";
        target_flags += "    - -D_LIBCPP_HAS_MUSL_LIBC=0\n";
        target_flags += "    - -D_LIBCPP_PSTL_BACKEND_SERIAL=1\n";
        target_flags += "    - -D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0\n";
        if (rw::platform::b_IsWindows)
        {
            target_flags += "    - -D__MINGW32__=1\n";
            target_flags += "    - -D_WIN32_WINNT=0x0a00\n";
        }
    }

    std::string zig_lines;
    if (rw::platform::b_IsWindows)
    {
        std::vector<fs::path> zig_candidates = { zig_libcxx, zig_libcxxabi };
        const std::vector<fs::path> libc_dirs = ZigLibcIncludeDirs(zig_root);
        zig_candidates.insert(zig_candidates.end(), libc_dirs.begin(), libc_dirs.end());
        for (const fs::path& candidate : zig_candidates)
        {
            if (fs::exists(candidate))
            {
                zig_lines += "    - \"-isystem" + ToForwardSlashes(candidate) + "\"\n";
            }
        }
    }

    std::string includes;
    includes += "    - \"-I" + ToForwardSlashes(engine_dir) + "\"\n";
    includes += "    - \"-I" + ToForwardSlashes(engine_dir / "Engine") + "\"\n";
    if (fs::exists(raylib_include))
    {
        includes += "    - \"-I" + ToForwardSlashes(raylib_include) + "\"\n";
    }

    std::string content = template_text;
    content = ReplaceToken(content, "@ZIG_TARGET_FLAGS@", target_flags);
    content = ReplaceToken(content, "@ZIG_ISYSTEMS@", zig_lines);
    content = ReplaceToken(content, "@PROJECT_INCLUDES@", includes);
    return content;
}

std::string BuildCppPropertiesContent(const fs::path& engine_dir, const fs::path& raylib_include,
    const fs::path& zig_root, const std::string& template_text)
{
    std::vector<fs::path> includes = { engine_dir, engine_dir / "Engine" };
    if (fs::exists(raylib_include))
    {
        includes.push_back(raylib_include);
    }

    if (rw::platform::b_IsWindows)
    {
        includes.push_back(zig_root / "lib" / "libcxx" / "include");
        includes.push_back(zig_root / "lib" / "libcxxabi" / "include");
        std::vector<fs::path> zig_candidates = ZigLibcIncludeDirs(zig_root);
        includes.insert(includes.end(), zig_candidates.begin(), zig_candidates.end());
    }

    std::vector<fs::path> existing;
    for (const fs::path& candidate : includes)
    {
        if (fs::exists(candidate))
        {
            existing.push_back(candidate);
        }
    }
    includes = std::move(existing);

    std::string paths;
    for (size_t index = 0; index < includes.size(); ++index)
    {
        if (index > 0)
        {
            paths += ",\n";
        }
        paths += "                " + JsonPath(includes[index]);
    }

    std::string content = template_text;
    content = ReplaceToken(content, "@INCLUDE_PATHS@", paths);
    return content;
}

// Locate the packaged agent CLI. Dev builds keep RayWaves.exe and rwcli.exe in
// the same directory; distributions do the same at the package root.
fs::path FindRwcliExecutable(const fs::path& exe_dir, const fs::path& engine_root)
{
    const std::string rwcli_name = rw::platform::ExecutableName("rwcli");
    const fs::path candidates[] = {
        exe_dir / rwcli_name,
        exe_dir / "Core" / rwcli_name,
        engine_root / rwcli_name,
        engine_root / "build" / "zig-release" / rwcli_name,
        engine_root / "build" / "zig-debug" / rwcli_name,
    };
    for (const fs::path& candidate : candidates)
    {
        if (fs::exists(candidate))
        {
            std::error_code error;
            const fs::path canonical = fs::weakly_canonical(candidate, error);
            return error ? candidate : canonical;
        }
    }
    return {};
}

// Batch files expand %, so a literal percent must be doubled.
std::string BatchPath(const fs::path& path)
{
    std::string text = path.string();
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (const char character : text)
    {
        if (character == '%')
        {
            escaped += "%%";
        }
        else
        {
            escaped += character;
        }
    }
    return escaped;
}

std::string ShellQuote(std::string_view value)
{
    std::string quoted = "'";
    for (const char character : value)
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

void MakeExecutable(const fs::path& path)
{
    std::error_code error;
    fs::permissions(
        path, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, error);
}

bool WriteToolingFiles(
    const fs::path& project_root, const t_Project& project, const fs::path& exe_dir, const fs::path& engine_root)
{
    const fs::path rwcli = FindRwcliExecutable(exe_dir, engine_root);
    const fs::path package_root = rwcli.empty() ? exe_dir : rwcli.parent_path();
    const fs::path tooling_dir = project_root / ".raywaves";
    const fs::path bin_dir = tooling_dir / "bin";

    std::string ini;
    ini += "[rwcli]\n";
    ini += "available=";
    ini += rwcli.empty() ? "0" : "1";
    ini += "\npath=" + rwcli.string() + "\n";
    ini += "packageRoot=" + package_root.string() + "\n";
    ini += "engineRoot=" + engine_root.string() + "\n";
    ini += "version=";
    ini += RAYWAVES_VERSION;
    ini += "\n";
    ini += "\n[project]\n";
    ini += "root=" + project_root.string() + "\n";
    ini += "sourceDir=" + project.m_SourceDir + "\n";
    ini += "assetDir=" + project.m_AssetDir + "\n";
    ini += "entryDll=" + project.m_EntryDll + "\n";
    const bool b_ini_written = b_WriteText(tooling_dir / "tooling.ini", ini, true);

    // Windows wrapper.
    std::string cmd_wrapper;
    cmd_wrapper += "@echo off\n";
    cmd_wrapper += "setlocal\n";
    cmd_wrapper += "set \"RWCLI=" + BatchPath(rwcli) + "\"\n";
    cmd_wrapper += "if not defined RWCLI goto locate\n";
    cmd_wrapper += "if exist \"%RWCLI%\" goto run\n";
    cmd_wrapper += ":locate\n";
    cmd_wrapper += "where rwcli.exe >nul 2>nul\n";
    cmd_wrapper += "if errorlevel 1 (\n";
    cmd_wrapper += "  echo rwcli not found. Reopen the project in RayWaves or run rwcli lsp. 1>&2\n";
    cmd_wrapper += "  exit /b 3\n";
    cmd_wrapper += ")\n";
    cmd_wrapper += "set \"RWCLI=rwcli.exe\"\n";
    cmd_wrapper += ":run\n";
    cmd_wrapper += "\"%RWCLI%\" %*\n";
    cmd_wrapper += "exit /b %ERRORLEVEL%\n";
    const bool b_cmd_written = b_WriteText(bin_dir / "rwcli.cmd", cmd_wrapper, true);

    // POSIX wrapper. Written on every host so a project moved between Windows
    // and Linux has a usable wrapper before the editor reopens it.
    std::string sh_wrapper;
    sh_wrapper += "#!/bin/sh\n";
    sh_wrapper += "RWCLI=" + ShellQuote(rwcli.string()) + "\n";
    sh_wrapper += "if [ -n \"$RWCLI\" ] && [ -x \"$RWCLI\" ]; then exec \"$RWCLI\" \"$@\"; fi\n";
    sh_wrapper += "if command -v rwcli >/dev/null 2>&1; then exec rwcli \"$@\"; fi\n";
    sh_wrapper += "echo \"rwcli not found. Reopen the project in RayWaves or run rwcli lsp.\" >&2\n";
    sh_wrapper += "exit 3\n";
    const fs::path sh_path = bin_dir / "rwcli";
    const bool b_sh_written = b_WriteText(sh_path, sh_wrapper, true);
    if (b_sh_written)
    {
        MakeExecutable(sh_path);
    }

    return b_ini_written && b_cmd_written && b_sh_written;
}

// Writes AGENTS.md, the project skill, and editor configs only when missing so
// a project owner can edit and keep them. rwcli lsp --force refreshes.
bool b_WriteProjectFiles(const fs::path& project_root, const t_Project& project, bool b_force)
{
    const fs::path exe_dir = PlatformInfo::GetExecutablePath().parent_path();
    const fs::path engine_root = ProjectManager::GetEngineRootDirectory();

    fs::path engine_dir = engine_root;
    if (fs::exists(engine_root / "Core" / "Engine"))
    {
        engine_dir = engine_root / "Core";
    }

    fs::path raylib_include = exe_dir / "raylib" / "include";
    if (!fs::exists(raylib_include / "raylib.h"))
    {
        raylib_include = exe_dir / "Core" / "raylib" / "include";
    }
    if (!fs::exists(raylib_include / "raylib.h"))
    {
        raylib_include = exe_dir / "_deps" / "raylib-build" / "raylib" / "include";
    }
    if (!fs::exists(raylib_include / "raylib.h"))
    {
        raylib_include.clear();
    }

    const fs::path zig_root = ProjectManager::GetToolsDirectory() / "zig";
    const auto tokens = [&project, &engine_root](std::string content)
    {
        content = ReplaceToken(content, "@PROJECT_NAME@", project.m_Name);
        content = ReplaceToken(content, "@SOURCE_DIR@", project.m_SourceDir);
        content = ReplaceToken(content, "@ASSET_DIR@", project.m_AssetDir);
        content = ReplaceToken(content, "@ENGINE_VERSION@", RAYWAVES_VERSION);
        content = ReplaceToken(content, "@ENGINE_ROOT@", ToForwardSlashes(engine_root));
        content = ReplaceToken(content, "@RWCLI_WRAPPER@",
            rw::platform::b_IsWindows ? ".raywaves\\bin\\rwcli.cmd" : ".raywaves/bin/rwcli");
        content = ReplaceToken(content, "@GAMELOGIC_MODULE@", rw::platform::c_GameLogicModuleName);
        return content;
    };

    bool b_ok = true;
    const fs::path agent_template = FindShippedFile(exe_dir, engine_root, "Project.AGENTS.md.in");
    if (!agent_template.empty())
    {
        b_ok &= b_WriteText(project_root / "AGENTS.md", tokens(ReadTemplate(agent_template)), false);
    }

    const fs::path skill_template = FindShippedFile(exe_dir, engine_root, "Project.SKILL.md.in");
    if (!skill_template.empty())
    {
        b_ok &= b_WriteText(project_root / ".agents" / "skills" / "raywaves-game" / "SKILL.md",
            tokens(ReadTemplate(skill_template)), false);
    }

    const fs::path reference_template = FindShippedFile(exe_dir, engine_root, "Project.rwcli-reference.md.in");
    if (!reference_template.empty())
    {
        b_ok &= b_WriteText(project_root / ".agents" / "skills" / "raywaves-game" / "reference.md",
            tokens(ReadTemplate(reference_template)), false);
    }

    const fs::path clangd_template = FindShippedFile(exe_dir, engine_root, "Project.clangd.in");
    if (!clangd_template.empty())
    {
        const std::string content =
            BuildClangdContent(engine_dir, raylib_include, zig_root, ReadTemplate(clangd_template));
        b_ok &= b_WriteText(project_root / ".clangd", content, b_force);
    }

    const fs::path properties_template =
        FindShippedFile(exe_dir, engine_root, "Project.vscode.c_cpp_properties.json.in");
    if (!properties_template.empty())
    {
        std::string content =
            BuildCppPropertiesContent(engine_dir, raylib_include, zig_root, ReadTemplate(properties_template));
        content = ReplaceToken(
            content, "@INTELLISENSE_MODE@", rw::platform::b_IsWindows ? "windows-clang-x64" : "linux-clang-x64");
        b_ok &= b_WriteText(project_root / ".vscode" / "c_cpp_properties.json", content, b_force);
    }

    const fs::path settings_template = FindShippedFile(exe_dir, engine_root, "Project.vscode.settings.json.in");
    if (!settings_template.empty())
    {
        std::string settings = ReadTemplate(settings_template);
        settings = ReplaceToken(settings, "@PACKAGE_ROOT@", ToForwardSlashes(exe_dir));
        settings = ReplaceToken(settings, "@TERMINAL_ENV_PLATFORM@", rw::platform::b_IsWindows ? "windows" : "linux");
        settings = ReplaceToken(settings, "@PATH_SEPARATOR@", std::string(1, rw::platform::c_PathListSeparator));
        b_ok &= b_WriteText(project_root / ".vscode" / "settings.json", settings, b_force);
    }

    const fs::path extensions_template = FindShippedFile(exe_dir, engine_root, "Project.vscode.extensions.json.in");
    if (!extensions_template.empty())
    {
        b_ok &= b_WriteText(project_root / ".vscode" / "extensions.json", ReadTemplate(extensions_template), b_force);
    }

    fs::path format_source = engine_root / ".clang-format";
    if (!fs::exists(format_source) && fs::exists(engine_root / "Core" / ".clang-format"))
    {
        format_source = engine_root / "Core" / ".clang-format";
    }
    if (fs::exists(format_source))
    {
        b_ok &= b_WriteText(project_root / ".clang-format", ReadTemplate(format_source), b_force);
    }

    b_ok &= WriteToolingFiles(project_root, project, exe_dir, engine_root);
    EnsureGitignoreEntries(project_root);
    return b_ok;
}
} // namespace

bool ProjectManager::GenerateCMakeLists()
{
    if (!b_HasOpenProject())
        return false;

    fs::path raywaves_dir = fs::path(s_Current.m_RootPath) / ".raywaves";
    fs::path cmake_path = raywaves_dir / "CMakeLists.txt";

    fs::path engine_root = GetEngineRootDirectory();
    fs::path engine_dir = engine_root;
    if (fs::exists(engine_root / "Core" / "Engine"))
    {
        engine_dir = engine_root / "Core";
    }

    // Resolve raylib path relative to exe (staged by main build or dist layout)
    const fs::path exe_dir = PlatformInfo::GetExecutablePath().parent_path();
    fs::path raylib_dir = exe_dir / "raylib";
    if (fs::exists(exe_dir / "Core" / "raylib" / "include" / "raylib.h"))
    {
        raylib_dir = exe_dir / "Core" / "raylib";
    }

    // Older build trees predate raylib staging and leave raylib/lib empty.
    // Fall back to the FetchContent build tree so project builds keep working
    // until the engine is rebuilt.
    auto has_raylib_import = [](const fs::path& dir)
    {
        return fs::exists(dir / "lib" / "libraylib.a") || fs::exists(dir / "lib" / "libraylib.dll.a") ||
            fs::exists(dir / "lib" / "libraylib.so") || fs::exists(dir / "lib" / "raylib.lib") ||
            fs::exists(dir / "libraylib.dll.a") || fs::exists(dir / "libraylib.a") ||
            fs::exists(dir / "libraylib.so") || fs::exists(dir / "raylib.lib");
    };
    if (!has_raylib_import(raylib_dir))
    {
        fs::path fetched = exe_dir / "_deps" / "raylib-build" / "raylib";
        if (has_raylib_import(fetched))
            raylib_dir = fetched;
    }

    fs::path template_path = FindProjectTemplate(exe_dir, engine_root);
    if (template_path.empty())
    {
        std::cerr << "Project CMake template not found. Expected packaging/Project.CMakeLists.in "
                     "next to the editor or in Core/.\n";
        return false;
    }

    std::ifstream template_file(template_path);
    if (!template_file.is_open())
        return false;

    std::string content((std::istreambuf_iterator<char>(template_file)), std::istreambuf_iterator<char>());

    content = ReplaceToken(content, "@PROJECT_NAME@", SanitizeCMakeProjectName(s_Current.m_Name));
    content = ReplaceToken(content, "@SOURCE_DIR@", ToForwardSlashes(s_Current.m_SourceDir));
    content = ReplaceToken(content, "@TOOLS_DIR@", ToForwardSlashes(GetToolsDirectory()));
    content = ReplaceToken(content, "@ENGINE_ROOT@", ToForwardSlashes(engine_root));
    content = ReplaceToken(content, "@ENGINE_DIR@", ToForwardSlashes(engine_dir));
    content = ReplaceToken(content, "@RAYLIB_DIR@", ToForwardSlashes(raylib_dir));

    if (!rw::platform::b_IsWindows)
    {
        const fs::path c_compiler = PlatformInfo::FindExecutable("gcc");
        const fs::path cxx_compiler = PlatformInfo::FindExecutable("g++");
        if (c_compiler.empty() || cxx_compiler.empty())
        {
            std::cerr << "GCC/G++ not found. Install build-essential before opening this project.\n";
            return false;
        }
        content = ReplaceToken(content, "@C_COMPILER@", ToForwardSlashes(c_compiler));
        content = ReplaceToken(content, "@CXX_COMPILER@", ToForwardSlashes(cxx_compiler));
    }

    std::ofstream file(cmake_path);
    if (!file.is_open())
        return false;
    file << content;
    return file.good();
}

bool ProjectManager::b_WriteIdeConfigs(bool b_force)
{
    std::scoped_lock lock(s_Mutex);
    if (!s_bOpen)
    {
        return false;
    }
    return b_WriteProjectFiles(fs::path(s_Current.m_RootPath), s_Current, b_force);
}
