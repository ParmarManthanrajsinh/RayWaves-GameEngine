#include "Engine/rw_timer.h"
#include "doctest/doctest.h"

TEST_CASE("rw::Timer: one-shot counts down and finishes once")
{
    rw::Timer timer(1.0f);
    CHECK(timer.b_IsRunning());
    CHECK_FALSE(timer.b_IsDone());
    CHECK(timer.Duration() == doctest::Approx(1.0f));
    CHECK(timer.Remaining() == doctest::Approx(1.0f));

    timer.Update(0.4f);
    CHECK(timer.Elapsed() == doctest::Approx(0.4f));
    CHECK(timer.Remaining() == doctest::Approx(0.6f));
    CHECK(timer.Progress() == doctest::Approx(0.4f));

    timer.Update(0.6f);
    CHECK_FALSE(timer.b_IsRunning());
    CHECK(timer.b_IsDone());
    CHECK(timer.Elapsed() == doctest::Approx(1.0f));
    CHECK(timer.Remaining() == doctest::Approx(0.0f));
    CHECK(timer.Progress() == doctest::Approx(1.0f));

    // A finished one-shot timer does not silently restart.
    timer.Update(5.0f);
    CHECK_FALSE(timer.b_IsRunning());
    CHECK(timer.Elapsed() == doctest::Approx(1.0f));
}

TEST_CASE("rw::Timer: restart and start reset the clock")
{
    rw::Timer timer(2.0f);
    timer.Update(1.5f);
    CHECK(timer.Elapsed() == doctest::Approx(1.5f));

    timer.Restart();
    CHECK(timer.b_IsRunning());
    CHECK(timer.Elapsed() == doctest::Approx(0.0f));

    timer.Update(2.5f);
    CHECK(timer.b_IsDone());

    timer.Start(0.5f);
    CHECK(timer.b_IsRunning());
    CHECK_FALSE(timer.b_IsDone());
    CHECK(timer.Duration() == doctest::Approx(0.5f));
    CHECK(timer.Elapsed() == doctest::Approx(0.0f));
}

TEST_CASE("rw::Timer: stop freezes without marking the timer done")
{
    rw::Timer timer(1.0f);
    timer.Update(0.25f);
    timer.Stop();

    CHECK_FALSE(timer.b_IsRunning());
    CHECK_FALSE(timer.b_IsDone());
    timer.Update(1.0f);
    CHECK(timer.Elapsed() == doctest::Approx(0.25f));

    timer.Restart();
    CHECK(timer.b_IsRunning());
    CHECK(timer.Elapsed() == doctest::Approx(0.0f));
}

TEST_CASE("rw::Timer: repeating timer wraps and keeps running")
{
    rw::Timer timer(1.0f, true);
    CHECK(timer.b_IsRepeating());

    timer.Update(2.25f);
    CHECK(timer.b_IsRunning());
    CHECK_FALSE(timer.b_IsDone());
    CHECK(timer.Elapsed() == doctest::Approx(0.25f));
    CHECK(timer.Progress() == doctest::Approx(0.25f));

    timer.Update(0.75f);
    CHECK(timer.Elapsed() == doctest::Approx(0.0f));
    CHECK(timer.b_IsRunning());
}

TEST_CASE("rw::Timer: zero duration is immediately done")
{
    rw::Timer timer(0.0f);
    CHECK_FALSE(timer.b_IsRunning());
    CHECK(timer.b_IsDone());
    CHECK(timer.Progress() == doctest::Approx(1.0f));

    rw::Timer negative(-5.0f);
    CHECK(negative.Duration() == doctest::Approx(0.0f));
    CHECK(negative.b_IsDone());
}

TEST_CASE("rw::Timer: non-positive updates are ignored")
{
    rw::Timer timer(1.0f);
    timer.Update(0.0f);
    timer.Update(-1.0f);
    CHECK(timer.Elapsed() == doctest::Approx(0.0f));
    CHECK(timer.b_IsRunning());
}

TEST_CASE("rw::Easing: endpoints and known curves")
{
    CHECK(rw::Tween::Ease(rw::Easing::Linear, 0.0f) == doctest::Approx(0.0f));
    CHECK(rw::Tween::Ease(rw::Easing::Linear, 1.0f) == doctest::Approx(1.0f));
    CHECK(rw::Tween::Ease(rw::Easing::EaseIn, 0.5f) == doctest::Approx(0.25f));
    CHECK(rw::Tween::Ease(rw::Easing::EaseOut, 0.5f) == doctest::Approx(0.75f));
    CHECK(rw::Tween::Ease(rw::Easing::EaseInOut, 0.25f) == doctest::Approx(0.125f));
    CHECK(rw::Tween::Ease(rw::Easing::SmoothStep, 0.5f) == doctest::Approx(0.5f));

    // Inputs outside [0, 1] clamp instead of overshooting.
    CHECK(rw::Tween::Ease(rw::Easing::Linear, -2.0f) == doctest::Approx(0.0f));
    CHECK(rw::Tween::Ease(rw::Easing::Linear, 2.0f) == doctest::Approx(1.0f));
}

TEST_CASE("rw::Tween: drives a float and finishes at the target")
{
    float value = 0.0f;
    rw::Tween tween(&value, 10.0f, 20.0f, 1.0f, rw::Easing::Linear);

    CHECK(tween.b_IsPlaying());
    CHECK_FALSE(tween.b_IsComplete());
    CHECK(value == doctest::Approx(10.0f));

    tween.Update(0.25f);
    CHECK(value == doctest::Approx(12.5f));
    CHECK(tween.Progress() == doctest::Approx(0.25f));

    tween.Update(0.75f);
    CHECK_FALSE(tween.b_IsPlaying());
    CHECK(tween.b_IsComplete());
    CHECK(value == doctest::Approx(20.0f));
    CHECK(tween.Progress() == doctest::Approx(1.0f));

    // Completion is sticky until Restart.
    tween.Update(1.0f);
    CHECK(value == doctest::Approx(20.0f));
}

TEST_CASE("rw::Tween: restart rewrites the start value and stop freezes progress")
{
    float value = 0.0f;
    rw::Tween tween(&value, 1.0f, 5.0f, 2.0f, rw::Easing::Linear);

    tween.Update(0.5f);
    const float midway = value;
    tween.Stop();
    tween.Update(1.0f);
    CHECK(value == doctest::Approx(midway));

    tween.Restart();
    CHECK(tween.b_IsPlaying());
    CHECK_FALSE(tween.b_IsComplete());
    CHECK(value == doctest::Approx(1.0f));
}

TEST_CASE("rw::Tween: looping wraps time and keeps playing")
{
    float value = 0.0f;
    rw::Tween tween(&value, 0.0f, 10.0f, 1.0f, rw::Easing::Linear, true);

    tween.Update(1.25f);
    CHECK(tween.b_IsPlaying());
    CHECK(tween.Elapsed() == doctest::Approx(0.25f));
    CHECK(value == doctest::Approx(2.5f));

    tween.Update(0.75f);
    CHECK(tween.Elapsed() == doctest::Approx(0.0f));
    CHECK(value == doctest::Approx(0.0f));
}

TEST_CASE("rw::Tween: zero duration snaps to the target")
{
    float value = 0.0f;
    rw::Tween tween(&value, 0.0f, 8.0f, 0.0f);

    CHECK(value == doctest::Approx(8.0f));
    CHECK_FALSE(tween.b_IsPlaying());
    CHECK(tween.b_IsComplete());
    CHECK(tween.Progress() == doctest::Approx(1.0f));
}

TEST_CASE("rw::Tween: null target and non-positive updates are inert")
{
    rw::Tween tween;
    tween.Update(1.0f);
    CHECK_FALSE(tween.b_IsPlaying());

    float value = 0.0f;
    tween.Bind(&value, 0.0f, 4.0f, 1.0f);
    tween.Update(0.0f);
    tween.Update(-1.0f);
    CHECK(value == doctest::Approx(0.0f));
    CHECK(tween.b_IsPlaying());
}
