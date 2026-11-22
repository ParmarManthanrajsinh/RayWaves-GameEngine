#pragma once
#include "IEditorPanel.h"
#include <cstddef>
#include <cstdint>
#include <string>

class SceneHierarchyPanel : public IEditorPanel
{
public:
    SceneHierarchyPanel() = default;
    ~SceneHierarchyPanel() override = default;

    void Draw(GameEditor* editor) override;

private:
    uint32_t m_LastSceneId = 0;
    uint64_t m_LastEntityId = 0;
    std::size_t m_LastSelectionCount = 0;
    bool m_bScrollToSelection = false;
    std::string m_Search;

    uint32_t m_RenamingSceneId = 0;
    std::string m_RenameOriginal;
    char m_RenameBuffer[128] = {};
};
