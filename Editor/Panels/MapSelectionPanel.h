#pragma once
#include "IEditorPanel.h"

class MapSelectionPanel : public IEditorPanel
{
public:
    MapSelectionPanel() = default;
    ~MapSelectionPanel() override = default;

    void Draw(GameEditor* editor) override;
};
