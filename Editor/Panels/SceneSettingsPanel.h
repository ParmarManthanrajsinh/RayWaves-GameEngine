#pragma once
#include "IEditorPanel.h"

class SceneSettingsPanel : public IEditorPanel
{
public:
    SceneSettingsPanel() = default;
    ~SceneSettingsPanel() override = default;

    void Draw(GameEditor* editor) override;
};
