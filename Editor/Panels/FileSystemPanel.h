#pragma once
#include "IEditorPanel.h"
#include <cstdint>
#include <filesystem>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

class MapManager;

class FileSystemPanel : public IEditorPanel
{
public:
    FileSystemPanel() = default;
    ~FileSystemPanel() override;

    void Draw(GameEditor* editor) override;

private:
    struct FThumbnail
    {
        Texture2D m_Texture{};
        std::filesystem::file_time_type m_WriteTime{};
    };

    // A directory snapshot stays valid until the directory's write time moves,
    // the directory itself is swapped out, or this panel mutates an asset.
    struct FDirectoryCache
    {
        std::filesystem::path m_Path;
        std::filesystem::file_time_type m_WriteTime{};
        std::vector<std::filesystem::path> m_Files;
        uint64_t m_RefreshFrame = 0;
        bool m_bValid = false;
        bool m_bExists = false;
    };

    struct FMissingAssetEntry
    {
        uint32_t m_ObjectId = 0;
        std::string m_ObjectName;
        std::string m_Path;
    };

    void DrawLevels(GameEditor* editor, MapManager* manager, const std::filesystem::path& asset_root);
    void DrawAssets(GameEditor* editor, const std::filesystem::path& asset_root);
    void DrawCode(const std::filesystem::path& source_root);
    void DrawMissingAssets(GameEditor* editor, const std::filesystem::path& asset_root);

    // Consumes raylib's pending OS file drop. Only called while this panel is
    // open, so drops are ignored when the panel is hidden or collapsed.
    void HandleFileDrop(GameEditor* editor);

    void StartRename(const std::filesystem::path& asset);
    void CommitRename(GameEditor* editor, const std::filesystem::path& asset);

    Texture2D* GetThumbnail(const std::filesystem::path& path);
    void UnloadThumbnails();

    // Returns the sorted listing for `directory`, re-reading it only when the
    // directory timestamp or identity changes. `max_age_frames` forces a
    // periodic re-read for trees whose nested changes never touch this
    // directory's own timestamp.
    const std::vector<std::filesystem::path>& GetCachedFiles(
        FDirectoryCache& cache, const std::filesystem::path& directory, bool b_recursive, uint64_t max_age_frames = 0);
    void InvalidateFileCaches();

    void RefreshMissingAssets(GameEditor* editor, const std::filesystem::path& asset_root);

    std::unordered_map<std::string, FThumbnail> m_Thumbnails;
    std::string m_Filter;
    std::string m_LastAssetRoot;
    std::string m_RenamingAssetPath;
    std::string m_RenameBuffer;
    bool m_bRenameFocusPending = false;
    FDirectoryCache m_AssetFiles;
    FDirectoryCache m_SourceFiles;
    std::vector<FMissingAssetEntry> m_MissingAssets;
    std::filesystem::path m_MissingScanRoot;
    uint64_t m_FrameCounter = 0;
    uint64_t m_MissingScanFrame = 0;
    bool m_bMissingScanValid = false;
};
