#pragma once
#include "IEditorPanel.h"

class SceneSettingsPanel : public IEditorPanel
{
public:
    SceneSettingsPanel() = default;
    ~SceneSettingsPanel() override = default;

    void Draw(GameEditor* editor) override;

private:
    int m_PrevWidth = 0;
    int m_PrevHeight = 0;
    int m_PrevTargetFPS = 0;
    bool m_bInitialized = false;
};
