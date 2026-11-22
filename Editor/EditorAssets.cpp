#include "EditorAssets.h"
#include "../Engine/ProjectManager.h"
#include <algorithm>
#include <cctype>

namespace
{
std::string LowerExtension(std::string_view path)
{
    const std::filesystem::path file_path{ std::string(path) };
    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

// Windows rejects these characters in file names, and the same set keeps names
// portable to Linux checkouts (where only '/' and NUL are special).
bool b_HasUnsafeFilenameChar(std::string_view name)
{
    for (const char character : name)
    {
        if (static_cast<unsigned char>(character) < 0x20)
        {
            return true;
        }
        switch (character)
        {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                return true;
            default:
                break;
        }
    }
    return false;
}
} // namespace

namespace EditorAssets
{
bool b_IsImage(std::string_view path)
{
    const std::string extension = LowerExtension(path);
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
        extension == ".tga" || extension == ".gif" || extension == ".qoi" || extension == ".psd";
}

bool b_IsAudio(std::string_view path)
{
    const std::string extension = LowerExtension(path);
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3" || extension == ".flac" ||
        extension == ".mod" || extension == ".xm";
}

bool b_IsCode(std::string_view path)
{
    const std::string extension = LowerExtension(path);
    return extension == ".cpp" || extension == ".h" || extension == ".hpp" || extension == ".c" || extension == ".cc" ||
        extension == ".cxx" || extension == ".cmake" || extension == ".txt";
}

bool b_IsScene(std::string_view path)
{
    return LowerExtension(path) == ".scene";
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& directory, bool recursive)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::exists(directory, error))
    {
        return files;
    }

    if (recursive)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error))
        {
            if (!error && entry.is_regular_file(error) && !entry.path().filename().string().starts_with("."))
            {
                files.push_back(entry.path());
            }
        }
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(directory, error))
        {
            if (!error && entry.is_regular_file(error) && !entry.path().filename().string().starts_with("."))
            {
                files.push_back(entry.path());
            }
        }
    }

    std::ranges::sort(files);
    return files;
}

std::string ToRelativeAssetPath(const std::filesystem::path& absolute_path)
{
    if (!ProjectManager::b_HasOpenProject())
    {
        return {};
    }

    const std::filesystem::path root = ProjectManager::GetCurrentCopy().m_AssetPath;
    if (root.empty())
    {
        return {};
    }

    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(absolute_path, root, error);
    if (error || relative.empty())
    {
        return {};
    }

    std::string text = relative.generic_string();
    if (text == ".." || text.starts_with("../"))
    {
        return {};
    }
    return text;
}

bool b_ImportFile(const std::filesystem::path& source, std::string& out_relative_path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error))
    {
        return false;
    }

    const std::string existing = ToRelativeAssetPath(source);
    if (!existing.empty())
    {
        out_relative_path = existing;
        return true;
    }

    const std::filesystem::path root = ProjectManager::GetCurrentCopy().m_AssetPath;
    if (root.empty())
    {
        return false;
    }

    std::filesystem::create_directories(root, error);

    std::filesystem::path target = root / source.filename();
    const std::string stem = source.stem().string();
    const std::string extension = source.extension().string();
    for (int suffix = 1; std::filesystem::exists(target, error) && suffix < 1000; ++suffix)
    {
        std::string filename = stem;
        filename += '_';
        filename += std::to_string(suffix);
        filename += extension;
        target = root / filename;
    }

    if (!std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, error))
    {
        return false;
    }

    out_relative_path = target.filename().string();
    return true;
}

bool b_RenameAsset(const std::filesystem::path& asset, std::string_view new_filename,
    std::filesystem::path& out_new_path, std::string& out_error)
{
    // Copy first: callers may pass the same variable for asset and out_new_path.
    const std::filesystem::path source = asset;
    out_new_path.clear();
    out_error.clear();

    if (new_filename.empty())
    {
        out_error = "New file name is empty.";
        return false;
    }
    // Reject ".." anywhere: it is the only multi-character escape pattern that
    // can survive a bare-filename check on every filesystem we support.
    if (new_filename == "." || new_filename.find("..") != std::string_view::npos)
    {
        out_error = "New file name may not contain '..'.";
        return false;
    }
    if (b_HasUnsafeFilenameChar(new_filename))
    {
        out_error = "New file name contains a path separator or a reserved character (<>:\"/\\|?*).";
        return false;
    }
    // Windows silently strips trailing dots and spaces, which makes the name
    // ambiguous on disk; refuse instead of writing something unexpected.
    if (new_filename.back() == '.' || new_filename.back() == ' ')
    {
        out_error = "New file name may not end with a dot or a space.";
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(source, error))
    {
        out_error = "Asset does not exist: " + source.string();
        return false;
    }

    if (ToRelativeAssetPath(source).empty())
    {
        out_error = "Asset is not inside the project's Assets folder.";
        return false;
    }

    const std::filesystem::path target = source.parent_path() / std::filesystem::path(std::string(new_filename));

    std::error_code equivalent_error;
    if (std::filesystem::equivalent(source, target, equivalent_error) && !equivalent_error)
    {
        // Renaming to the current name is a no-op rather than a collision.
        std::error_code absolute_error;
        out_new_path = std::filesystem::absolute(source, absolute_error);
        if (absolute_error)
        {
            out_new_path = source;
        }
        return true;
    }

    error.clear();
    if (std::filesystem::exists(target, error))
    {
        out_error = "A file named '" + std::string(new_filename) + "' already exists.";
        return false;
    }

    std::filesystem::rename(source, target, error);
    if (error)
    {
        out_error = "Rename failed: " + error.message();
        return false;
    }

    std::error_code absolute_error;
    out_new_path = std::filesystem::absolute(target, absolute_error);
    if (absolute_error)
    {
        out_new_path = target;
    }
    return true;
}
} // namespace EditorAssets
