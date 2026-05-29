#pragma once
#include "IEditorPanel.h"

class MainMenuBar : public IEditorPanel
{
public:
    MainMenuBar() = default;
    ~MainMenuBar() override = default;

    void Draw(GameEditor* editor) override;
};
