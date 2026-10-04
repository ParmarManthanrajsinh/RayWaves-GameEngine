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
    static void DrawToolbarBackground();
    static bool s_bIconButton(std::string_view label, std::string_view icon, const struct ImVec2& size, std::string_view tooltip);
    static void s_fDrawSpinner(float radius, float thickness, const unsigned int& color);
};
