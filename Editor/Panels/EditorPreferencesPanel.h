#pragma once
#include "IEditorPanel.h"

class EditorPreferencesPanel : public IEditorPanel
{
public:
    void Draw(class GameEditor* editor) override;
    
private:
    float m_DraggingGuiScale = 1.0f;
    bool m_bIsDraggingScale = false;
};
