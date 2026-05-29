#pragma once
#include "IEditorPanel.h"

class MessageLogPanel : public IEditorPanel
{
public:
    MessageLogPanel() = default;
    ~MessageLogPanel() override = default;

    void Draw(GameEditor* editor) override;
};
