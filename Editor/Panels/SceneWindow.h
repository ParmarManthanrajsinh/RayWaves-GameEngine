#pragma once
#include "IEditorPanel.h"
#include <string>

class SceneWindow : public IEditorPanel
{
public:
    SceneWindow() = default;
    ~SceneWindow() override = default;

    void Draw(GameEditor* editor) override;

private:
    void DrawToolbarBackground();
    bool s_bIconButton(std::string_view label, std::string_view icon, const struct ImVec2& size, std::string_view tooltip);
    void s_fDrawSpinner(float radius, float thickness, const unsigned int& color);
};
