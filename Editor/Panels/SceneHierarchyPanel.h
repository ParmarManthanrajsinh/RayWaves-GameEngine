#pragma once
#include "IEditorPanel.h"
#include <cstdint>

class SceneHierarchyPanel : public IEditorPanel
{
public:
    SceneHierarchyPanel() = default;
    ~SceneHierarchyPanel() override = default;

    void Draw(GameEditor* editor) override;

private:
    uint32_t m_LastSceneId = 0;
    uint64_t m_LastEntityId = 0;
    bool m_bScrollToSelection = false;

    uint32_t m_RenamingSceneId = 0;
    char m_RenameBuffer[128] = {};
};
