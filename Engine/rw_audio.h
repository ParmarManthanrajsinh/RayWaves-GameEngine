#pragma once
#include "AssetResolver.h"
#include <raylib.h>
#include <string>
#include <string_view>
#include <utility>
namespace rw
{
// Owning raylib sound effect. Paths go through AssetResolver, so relative asset
// names work in both the editor and the standalone game. Audio handles are only
// created and released while the audio device is ready: CloseAudioDevice frees
// every buffer, so touching a stale handle afterwards would be a use-after-free.
class Sound
{
public:
    Sound() = default;
    explicit Sound(std::string_view path) { Load(path); }
    ~Sound() { Unload(); }

    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    Sound(Sound&& other) noexcept : m_Sound(other.m_Sound) { other.m_Sound = {}; }

    Sound& operator=(Sound&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_Sound = other.m_Sound;
            other.m_Sound = {};
        }
        return *this;
    }

    // Reload-safe: loading always drops the previous handle first, including
    // when the new path is empty or the device is not ready.
    bool Load(std::string_view path)
    {
        Unload();
        if (path.empty() || !::IsAudioDeviceReady())
        {
            return false;
        }

        const std::string resolved = AssetResolver::Resolve(path);
        if (!::FileExists(resolved.c_str()))
        {
            return false;
        }

        const ::Sound sound = ::LoadSound(resolved.c_str());
        if (!::IsSoundValid(sound))
        {
            return false;
        }
        m_Sound = sound;
        return true;
    }

    void Unload()
    {
        if (::IsAudioDeviceReady() && ::IsSoundValid(m_Sound))
        {
            ::UnloadSound(m_Sound);
        }
        m_Sound = {};
    }

    bool IsValid() const { return ::IsAudioDeviceReady() && ::IsSoundValid(m_Sound); }

    void Play() const
    {
        if (IsValid())
        {
            ::PlaySound(m_Sound);
        }
    }

    void Stop() const
    {
        if (IsValid())
        {
            ::StopSound(m_Sound);
        }
    }

    void SetVolume(float volume)
    {
        if (IsValid())
        {
            ::SetSoundVolume(m_Sound, volume);
        }
    }

    const ::Sound& Get() const { return m_Sound; }

private:
    ::Sound m_Sound{};
};

// Owning raylib music stream. Streaming music must be pumped once per frame
// while it plays; call Update() from the level's update hook.
class Music
{
public:
    Music() = default;
    explicit Music(std::string_view path) { Load(path); }
    ~Music() { Unload(); }

    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;

    Music(Music&& other) noexcept : m_Music(other.m_Music) { other.m_Music = {}; }

    Music& operator=(Music&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_Music = other.m_Music;
            other.m_Music = {};
        }
        return *this;
    }

    // Reload-safe, same contract as rw::Sound::Load.
    bool Load(std::string_view path)
    {
        Unload();
        if (path.empty() || !::IsAudioDeviceReady())
        {
            return false;
        }

        const std::string resolved = AssetResolver::Resolve(path);
        if (!::FileExists(resolved.c_str()))
        {
            return false;
        }

        const ::Music music = ::LoadMusicStream(resolved.c_str());
        if (!::IsMusicValid(music))
        {
            return false;
        }
        m_Music = music;
        return true;
    }

    void Unload()
    {
        if (::IsAudioDeviceReady() && ::IsMusicValid(m_Music))
        {
            ::UnloadMusicStream(m_Music);
        }
        m_Music = {};
    }

    bool IsValid() const { return ::IsAudioDeviceReady() && ::IsMusicValid(m_Music); }

    void Play()
    {
        if (IsValid())
        {
            ::PlayMusicStream(m_Music);
        }
    }

    void Pause()
    {
        if (IsValid())
        {
            ::PauseMusicStream(m_Music);
        }
    }

    void Resume()
    {
        if (IsValid())
        {
            ::ResumeMusicStream(m_Music);
        }
    }

    void Stop()
    {
        if (IsValid())
        {
            ::StopMusicStream(m_Music);
        }
    }

    // Pumps the stream buffers; raylib requires one call per frame while playing.
    void Update()
    {
        if (IsValid())
        {
            ::UpdateMusicStream(m_Music);
        }
    }

    void SetVolume(float volume)
    {
        if (IsValid())
        {
            ::SetMusicVolume(m_Music, volume);
        }
    }

    // Derived from the stream instead of cached, so pause, stop, and the end of
    // playback all report the true state.
    bool b_IsPlaying() const { return IsValid() && ::IsMusicStreamPlaying(m_Music); }

    const ::Music& Get() const { return m_Music; }

private:
    ::Music m_Music{};
};

// Owning raylib font. Unlike audio, fonts do not need an initialized device,
// but the missing-file case must stay inert: raylib can hand back its shared
// default font on failure and unloading that would break every DrawText call.
class Font
{
public:
    Font() = default;
    explicit Font(std::string_view path, int size = 32) : m_Size((size > 0) ? size : 32) { Load(path); }
    ~Font() { Unload(); }

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    Font(Font&& other) noexcept : m_Font(other.m_Font), m_Size(other.m_Size)
    {
        other.m_Font = {};
        other.m_Size = 32;
    }

    Font& operator=(Font&& other) noexcept
    {
        if (this != &other)
        {
            Unload();
            m_Font = other.m_Font;
            m_Size = other.m_Size;
            other.m_Font = {};
            other.m_Size = 32;
        }
        return *this;
    }

    // Reload-safe; reuses the size supplied at construction.
    bool Load(std::string_view path)
    {
        Unload();
        if (path.empty())
        {
            return false;
        }

        const std::string resolved = AssetResolver::Resolve(path);
        if (!::FileExists(resolved.c_str()))
        {
            return false;
        }

        const ::Font font = ::LoadFontEx(resolved.c_str(), m_Size, nullptr, 0);
        if (!::IsFontValid(font))
        {
            return false;
        }

        const ::Font default_font = ::GetFontDefault();
        if ((font.texture.id == default_font.texture.id) || (font.glyphs == default_font.glyphs))
        {
            return false;
        }

        m_Font = font;
        return true;
    }

    // Convenience overload that changes the size before loading.
    bool Load(std::string_view path, int size)
    {
        m_Size = (size > 0) ? size : 32;
        return Load(path);
    }

    void Unload()
    {
        if (::IsFontValid(m_Font))
        {
            ::UnloadFont(m_Font);
        }
        m_Font = {};
    }

    bool IsValid() const { return ::IsFontValid(m_Font); }

    int Size() const { return m_Size; }

    const ::Font& Get() const { return m_Font; }

    Vector2 Measure(std::string_view text, float spacing = 1.0f) const
    {
        if (text.empty() || !IsValid())
        {
            return {};
        }
        const std::string owned(text);
        return ::MeasureTextEx(m_Font, owned.c_str(), static_cast<float>(m_Size), spacing);
    }

private:
    ::Font m_Font{};
    int m_Size = 32;
};
} // namespace rw
