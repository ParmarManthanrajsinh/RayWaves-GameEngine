#pragma once
#include <algorithm>
#include <cmath>

namespace rw
{
// Countdown timer for cooldowns, spawn intervals, and simple state changes.
//
// Update() owns the clock. Finishing does not auto-reset unless the timer was
// created (or restarted) in repeating mode, so a one-shot timer stays done until
// Start() or Restart() is called again.
class Timer
{
public:
    Timer() = default;
    explicit Timer(float duration, bool b_repeat = false)
        : m_Duration(std::max(duration, 0.0f))
        , m_bRepeat(b_repeat)
        , m_bRunning(m_Duration > 0.0f)
        , m_bDone(m_Duration <= 0.0f)
    {
    }

    // Starts a new countdown. A duration of zero counts as immediately done.
    void Start(float duration, bool b_repeat = false)
    {
        m_Duration = std::max(duration, 0.0f);
        m_Elapsed = 0.0f;
        m_bRepeat = b_repeat;
        m_bRunning = m_Duration > 0.0f;
        m_bDone = m_Duration <= 0.0f;
    }

    void Restart()
    {
        m_Elapsed = 0.0f;
        m_bRunning = m_Duration > 0.0f;
        m_bDone = m_Duration <= 0.0f;
    }

    void Stop() { m_bRunning = false; }

    void Update(float delta_time)
    {
        if (!m_bRunning || delta_time <= 0.0f)
        {
            return;
        }

        m_Elapsed += delta_time;
        if (m_Elapsed < m_Duration)
        {
            return;
        }

        if (m_bRepeat)
        {
            m_Elapsed = std::fmod(m_Elapsed, m_Duration);
            m_bDone = false;
        }
        else
        {
            m_Elapsed = m_Duration;
            m_bRunning = false;
            m_bDone = true;
        }
    }

    bool b_IsRunning() const { return m_bRunning; }
    bool b_IsDone() const { return m_bDone; }
    bool b_IsRepeating() const { return m_bRepeat; }
    float Duration() const { return m_Duration; }
    float Elapsed() const { return m_Elapsed; }
    float Remaining() const { return std::max(m_Duration - m_Elapsed, 0.0f); }
    float Progress() const { return m_Duration <= 0.0f ? 1.0f : std::clamp(m_Elapsed / m_Duration, 0.0f, 1.0f); }

private:
    float m_Duration = 0.0f;
    float m_Elapsed = 0.0f;
    bool m_bRepeat = false;
    bool m_bRunning = false;
    bool m_bDone = true;
};

// Easing curves shared by Tween and gameplay code.
enum class Easing
{
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    SmoothStep
};

// Drives one float through an easing curve. The target pointer must outlive the
// tween; OnDraw or OnUpdate reads the value directly.
//
// A tween is deliberately small: bind it to a member (speed, alpha, camera
// offset), update once per frame, and keep gameplay code free of timers.
class Tween
{
public:
    Tween() = default;
    Tween(float* target, float from, float to, float duration, Easing easing = Easing::SmoothStep, bool b_loop = false)
    {
        Bind(target, from, to, duration, easing, b_loop);
    }

    void Bind(
        float* target, float from, float to, float duration, Easing easing = Easing::SmoothStep, bool b_loop = false)
    {
        m_pTarget = target;
        m_From = from;
        m_To = to;
        m_Duration = std::max(duration, 0.0f);
        m_Easing = easing;
        m_bLoop = b_loop;
        Restart();
    }

    // Restarts from the beginning and writes the start value immediately.
    void Restart()
    {
        m_Time = 0.0f;
        m_bPlaying = (m_pTarget != nullptr) && (m_Duration > 0.0f);
        m_bComplete = (m_pTarget != nullptr) && (m_Duration <= 0.0f);

        if (m_pTarget != nullptr)
        {
            *m_pTarget = (m_Duration > 0.0f) ? m_From : m_To;
        }
    }

    void Stop() { m_bPlaying = false; }

    void Update(float delta_time)
    {
        if (!m_bPlaying || m_pTarget == nullptr || delta_time <= 0.0f)
        {
            return;
        }

        m_Time += delta_time;
        if (m_bLoop)
        {
            m_Time = std::fmod(m_Time, m_Duration);
        }
        else if (m_Time >= m_Duration)
        {
            m_Time = m_Duration;
            m_bPlaying = false;
            m_bComplete = true;
        }

        *m_pTarget = m_From + ((m_To - m_From) * Ease(m_Easing, Progress()));
    }

    bool b_IsPlaying() const { return m_bPlaying; }
    bool b_IsComplete() const { return m_bComplete; }
    bool b_IsLooping() const { return m_bLoop; }
    float Duration() const { return m_Duration; }
    float Elapsed() const { return m_Time; }
    float Progress() const { return m_Duration <= 0.0f ? 1.0f : std::clamp(m_Time / m_Duration, 0.0f, 1.0f); }
    float Value() const
    {
        return m_pTarget == nullptr ? m_To : m_From + ((m_To - m_From) * Ease(m_Easing, Progress()));
    }

    static float Ease(Easing easing, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        switch (easing)
        {
            case Easing::Linear:
                return t;
            case Easing::EaseIn:
                return t * t;
            case Easing::EaseOut:
                return 1.0f - ((1.0f - t) * (1.0f - t));
            case Easing::EaseInOut:
                return (t < 0.5f) ? (2.0f * t * t) : (1.0f - (std::pow((-2.0f * t) + 2.0f, 2.0f) * 0.5f));
            case Easing::SmoothStep:
            default:
                return t * t * (3.0f - (2.0f * t));
        }
    }

private:
    float* m_pTarget = nullptr;
    float m_From = 0.0f;
    float m_To = 0.0f;
    float m_Duration = 0.0f;
    float m_Time = 0.0f;
    Easing m_Easing = Easing::SmoothStep;
    bool m_bLoop = false;
    bool m_bPlaying = false;
    bool m_bComplete = true;
};
} // namespace rw
