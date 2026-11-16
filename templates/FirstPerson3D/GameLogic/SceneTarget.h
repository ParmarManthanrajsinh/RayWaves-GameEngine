#pragma once
#include <raylib.h>

// 2x supersampling target (SSAA). Raylib render textures cannot request MSAA,
// so render the 3D scene at twice the viewport resolution and downsample with
// bilinear filtering. Works both in the editor scene texture and standalone.
class SceneTarget
{
public:
    SceneTarget() = default;
    ~SceneTarget();

    SceneTarget(const SceneTarget&) = delete;
    SceneTarget& operator=(const SceneTarget&) = delete;

    void Begin(int width, int height);
    void End();
    void Unload();

private:
    RenderTexture2D m_Target{};
    unsigned int m_PreviousFbo = 0;
    int m_PreviousWidth = 0;
    int m_PreviousHeight = 0;
    int m_Width = 0;
    int m_Height = 0;
    bool m_bActive = false;

    static constexpr int SCALE = 2;
};
