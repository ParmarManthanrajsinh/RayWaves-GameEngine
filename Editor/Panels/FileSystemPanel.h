#pragma once
#include "IEditorPanel.h"
#include <filesystem>
#include <raylib.h>
#include <string>
#include <unordered_map>

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

    void DrawLevels(GameEditor* editor, MapManager* manager, const std::filesystem::path& asset_root);
    void DrawAssets(GameEditor* editor, const std::filesystem::path& asset_root);
    void DrawCode(const std::filesystem::path& source_root);

    Texture2D* GetThumbnail(const std::filesystem::path& path);
    void UnloadThumbnails();

    std::unordered_map<std::string, FThumbnail> m_Thumbnails;
    std::string m_Filter;
    std::string m_LastAssetRoot;
};
