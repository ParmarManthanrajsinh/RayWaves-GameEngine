#pragma once
#include "IEditorPanel.h"

class InspectorPanel : public IEditorPanel
{
public:
    InspectorPanel() = default;
    ~InspectorPanel() override = default;

    void Draw(GameEditor* editor) override;
};
