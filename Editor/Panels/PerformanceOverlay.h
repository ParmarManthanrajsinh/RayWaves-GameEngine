#pragma once
#include "IEditorPanel.h"

class PerformanceOverlay : public IEditorPanel
{
public:
    PerformanceOverlay() = default;
    ~PerformanceOverlay() override = default;

    void Draw(GameEditor* editor) override;
};
