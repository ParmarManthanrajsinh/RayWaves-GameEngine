#include "SceneTarget.h"
#include <rlgl.h>

SceneTarget::~SceneTarget()
{
    Unload();
}

void SceneTarget::Begin(int width, int height)
{
    m_bActive = false;

    if ((width <= 0) || (height <= 0))
    {
        return;
    }

    if ((m_Target.id == 0) || (m_Width != width) || (m_Height != height))
    {
        Unload();
        m_Target = LoadRenderTexture(width * SCALE, height * SCALE);
        m_Width = width;
        m_Height = height;

        if (IsRenderTextureValid(m_Target))
        {
            SetTextureFilter(m_Target.texture, TEXTURE_FILTER_BILINEAR);
        }
    }

    if (!IsRenderTextureValid(m_Target))
    {
        return;
    }

    // DrawMap() may run inside the editor's scene render texture, so remember
    // the caller's framebuffer. End() restores it after the offscreen pass.
    m_PreviousFbo = rlGetActiveFramebuffer();
    m_PreviousWidth = rlGetFramebufferWidth();
    m_PreviousHeight = rlGetFramebufferHeight();

    BeginTextureMode(m_Target);
    m_bActive = true;
}

void SceneTarget::End()
{
    if (!m_bActive)
    {
        return;
    }
    m_bActive = false;

    EndTextureMode();

    // EndTextureMode always returns to the default framebuffer. In the editor
    // the caller target is the scene texture, so restore that FBO explicitly
    // instead of drawing the resolve pass to the window.
    if (m_PreviousFbo != 0)
    {
        rlEnableFramebuffer(m_PreviousFbo);
    }
    else
    {
        rlDisableFramebuffer();
    }

    rlViewport(0, 0, m_PreviousWidth, m_PreviousHeight);
    rlSetFramebufferWidth(m_PreviousWidth);
    rlSetFramebufferHeight(m_PreviousHeight);

    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0.0f, static_cast<float>(m_PreviousWidth), static_cast<float>(m_PreviousHeight), 0.0f, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();

    Rectangle source = {
        0.0f,
        0.0f,
        static_cast<float>(m_Target.texture.width),
        -static_cast<float>(m_Target.texture.height),
    };
    Rectangle destination = { 0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height) };

    DrawTexturePro(m_Target.texture, source, destination, { 0.0f, 0.0f }, 0.0f, WHITE);
}

void SceneTarget::Unload()
{
    m_bActive = false;

    if (m_Target.id != 0)
    {
        UnloadRenderTexture(m_Target);
        m_Target = {};
    }

    m_Width = 0;
    m_Height = 0;
}
