#pragma once

class GameEditor;

class IEditorPanel
{
public:
    virtual ~IEditorPanel() = default;
    
    // Draw the panel UI. The editor instance is passed so the panel can access state.
    virtual void Draw(GameEditor* editor) = 0;
};
