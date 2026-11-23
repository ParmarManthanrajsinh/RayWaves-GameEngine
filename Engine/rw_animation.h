#pragma once
#include "rw_core.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace rw
{
// One named range of a sprite sheet. m_StartFrame is an absolute frame index
// into the sheet; the animator tracks the clip-local offset on top of it.
struct AnimationClip
{
    std::string m_Name;
    int m_StartFrame = 0;
    int m_FrameCount = 1;
    float m_FramesPerSecond = 12.0f;
    bool m_bLoop = true;
};

// Uniform-grid sprite sheet. Frames are numbered left to right, top to bottom.
// Load() goes through AssetResolver, so relative asset names work in both the
// editor and the standalone game.
class SpriteSheet
{
public:
    SpriteSheet() = default;

    bool Load(std::string_view path, Vector2 frame_size)
    {
        Unload();
        if (!std::isfinite(frame_size.x) || !std::isfinite(frame_size.y) || frame_size.x <= 0.0f ||
            frame_size.y <= 0.0f)
        {
            return false;
        }

        m_FrameSize = frame_size;
        if (!m_Texture.Load(path))
        {
            m_FrameSize = { 0.0f, 0.0f };
            return false;
        }
        return true;
    }

    void Unload()
    {
        m_Texture.Unload();
        m_FrameSize = { 0.0f, 0.0f };
    }

    bool IsValid() const { return m_Texture.IsValid(); }
    Vector2 FrameSize() const { return m_FrameSize; }
    const rw::Texture& GetTexture() const { return m_Texture; }

    int Columns() const
    {
        const int frame_width = static_cast<int>(m_FrameSize.x);
        if (frame_width <= 0 || m_Texture.Width() <= 0)
        {
            return 0;
        }
        return m_Texture.Width() / frame_width;
    }

    // Source rectangle for an absolute frame index. Invalid sheets and indices
    // return an empty rectangle, so callers can draw unconditionally.
    Rectangle FrameRect(int index) const
    {
        const int columns = Columns();
        const int frame_height = static_cast<int>(m_FrameSize.y);
        if (columns <= 0 || frame_height <= 0 || m_Texture.Height() <= 0 || index < 0)
        {
            return {};
        }

        const int rows = m_Texture.Height() / frame_height;
        if (rows <= 0 || index >= columns * rows)
        {
            return {};
        }

        const int column = index % columns;
        const int row = index / columns;
        return Rectangle{ static_cast<float>(column) * m_FrameSize.x, static_cast<float>(row) * m_FrameSize.y,
            m_FrameSize.x, m_FrameSize.y };
    }

private:
    rw::Texture m_Texture;
    Vector2 m_FrameSize{ 0.0f, 0.0f };
};

// Sprite animation clock. It only does frame math and never touches the GPU, so
// it can be driven and tested without an initialized window.
class Animator
{
public:
    void AddClip(AnimationClip clip) { m_Clips.push_back(std::move(clip)); }

    void AddClip(std::string_view name, int start_frame, int frame_count, float frames_per_second, bool b_loop = true)
    {
        AddClip(AnimationClip{ std::string(name), start_frame, frame_count, frames_per_second, b_loop });
    }

    // Switching clips restarts the clock. Replaying the active clip is a no-op
    // so callers can request the current state on every frame.
    bool Play(std::string_view name)
    {
        const auto found = std::find_if(
            m_Clips.begin(), m_Clips.end(), [name](const AnimationClip& clip) { return clip.m_Name == name; });
        if (found == m_Clips.end())
        {
            return false;
        }

        const int index = static_cast<int>(found - m_Clips.begin());
        if (m_bPlaying && index == m_CurrentClip)
        {
            return true;
        }

        m_CurrentClip = index;
        m_FrameOffset = 0;
        m_FrameTime = 0.0f;
        m_bPlaying = true;
        return true;
    }

    // Freezes on the current frame. Play() restarts the clip afterwards.
    void Stop() { m_bPlaying = false; }

    void Update(float delta_time)
    {
        if (!m_bPlaying || m_CurrentClip < 0 || m_CurrentClip >= static_cast<int>(m_Clips.size()))
        {
            return;
        }

        const AnimationClip& clip = m_Clips[static_cast<size_t>(m_CurrentClip)];
        if (clip.m_FrameCount <= 0 || !std::isfinite(clip.m_FramesPerSecond) || clip.m_FramesPerSecond <= 0.0f ||
            !std::isfinite(delta_time) || delta_time <= 0.0f)
        {
            return;
        }

        const float frame_duration = 1.0f / clip.m_FramesPerSecond;
        const float clip_span = frame_duration * static_cast<float>(clip.m_FrameCount);
        m_FrameTime += delta_time;

        // Bound the accumulator before converting to a frame count, so a long
        // stall cannot overflow the advance step.
        if (clip.m_bLoop && m_FrameTime >= clip_span)
        {
            m_FrameTime = std::fmod(m_FrameTime, clip_span);
        }
        else if (m_FrameTime > clip_span)
        {
            m_FrameTime = clip_span;
        }

        const int advance = static_cast<int>(m_FrameTime / frame_duration);
        if (advance <= 0)
        {
            return;
        }
        m_FrameTime = std::max(m_FrameTime - (static_cast<float>(advance) * frame_duration), 0.0f);

        if (clip.m_bLoop)
        {
            m_FrameOffset = (m_FrameOffset + advance) % clip.m_FrameCount;
        }
        else if (m_FrameOffset + advance >= clip.m_FrameCount - 1)
        {
            m_FrameOffset = clip.m_FrameCount - 1;
            m_FrameTime = 0.0f;
            m_bPlaying = false;
        }
        else
        {
            m_FrameOffset += advance;
        }
    }

    // Absolute frame index into the sprite sheet, start frame included.
    int FrameIndex() const
    {
        if (m_CurrentClip < 0 || m_CurrentClip >= static_cast<int>(m_Clips.size()))
        {
            return 0;
        }
        return m_Clips[static_cast<size_t>(m_CurrentClip)].m_StartFrame + m_FrameOffset;
    }

    Rectangle FrameRect(const SpriteSheet& sheet) const { return sheet.FrameRect(FrameIndex()); }

    bool b_IsPlaying() const { return m_bPlaying; }

    std::string_view CurrentClip() const
    {
        if (m_CurrentClip < 0 || m_CurrentClip >= static_cast<int>(m_Clips.size()))
        {
            return {};
        }
        return m_Clips[static_cast<size_t>(m_CurrentClip)].m_Name;
    }

    // Seconds accumulated toward the next frame advance.
    float FrameTime() const { return m_FrameTime; }

private:
    std::vector<AnimationClip> m_Clips;
    int m_CurrentClip = -1;
    int m_FrameOffset = 0;
    float m_FrameTime = 0.0f;
    bool m_bPlaying = false;
};
} // namespace rw
