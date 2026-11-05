#pragma once

#include "IEditorPanel.h"

class TerminalPanel : public IEditorPanel
{
public:
    TerminalPanel() = default;
    ~TerminalPanel() override = default;

    void Draw(GameEditor* editor) override;
};
