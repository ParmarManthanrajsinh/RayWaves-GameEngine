#pragma once
#include "IEditorPanel.h"

class ExportPanel : public IEditorPanel
{
public:
    ExportPanel() = default;
    ~ExportPanel() override = default;

    void Draw(GameEditor* editor) override;
};
