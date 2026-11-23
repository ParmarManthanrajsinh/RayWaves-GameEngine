#include "Engine/rw_animation.h"
#include "doctest/doctest.h"

TEST_CASE("rw::AnimationClip: defaults")
{
    const rw::AnimationClip clip;
    CHECK(clip.m_Name.empty());
    CHECK(clip.m_StartFrame == 0);
    CHECK(clip.m_FrameCount == 1);
    CHECK(clip.m_FramesPerSecond == doctest::Approx(12.0f));
    CHECK(clip.m_bLoop);
}

TEST_CASE("rw::Animator: adding and playing a clip")
{
    rw::Animator animator;
    animator.AddClip("idle", 0, 4, 10.0f);

    CHECK(animator.FrameIndex() == 0);
    CHECK_FALSE(animator.b_IsPlaying());
    CHECK(animator.CurrentClip().empty());

    REQUIRE(animator.Play("idle"));
    CHECK(animator.b_IsPlaying());
    CHECK(animator.CurrentClip() == "idle");
    CHECK(animator.FrameIndex() == 0);
    CHECK(animator.FrameTime() == doctest::Approx(0.0f));
}

TEST_CASE("rw::Animator: AddClip accepts a clip struct")
{
    rw::Animator animator;
    animator.AddClip(rw::AnimationClip{ "walk", 4, 6, 8.0f, false });

    REQUIRE(animator.Play("walk"));
    CHECK(animator.FrameIndex() == 4);
    CHECK(animator.CurrentClip() == "walk");
}

TEST_CASE("rw::Animator: update advances frames at a fixed dt")
{
    rw::Animator animator;
    animator.AddClip("run", 0, 4, 10.0f);
    REQUIRE(animator.Play("run"));

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 1);

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 2);
}

TEST_CASE("rw::Animator: partial frame time accumulates")
{
    rw::Animator animator;
    animator.AddClip("run", 0, 4, 10.0f);
    REQUIRE(animator.Play("run"));

    animator.Update(0.05f);
    CHECK(animator.FrameIndex() == 0);
    CHECK(animator.FrameTime() == doctest::Approx(0.05f));

    animator.Update(0.05f);
    CHECK(animator.FrameIndex() == 1);
    CHECK(animator.FrameTime() == doctest::Approx(0.0f));
}

TEST_CASE("rw::Animator: looping clips wrap to the first frame")
{
    rw::Animator animator;
    animator.AddClip("cycle", 2, 3, 10.0f);
    REQUIRE(animator.Play("cycle"));
    CHECK(animator.FrameIndex() == 2);

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 3);

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 4);

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 2);
}

TEST_CASE("rw::Animator: non-looping clips hold the last frame")
{
    rw::Animator animator;
    animator.AddClip("die", 10, 3, 10.0f, false);
    REQUIRE(animator.Play("die"));

    for (int step = 0; step < 5; ++step)
    {
        animator.Update(0.1f);
    }

    CHECK(animator.FrameIndex() == 12);
    CHECK_FALSE(animator.b_IsPlaying());
}

TEST_CASE("rw::Animator: stop freezes the current frame")
{
    rw::Animator animator;
    animator.AddClip("run", 0, 4, 10.0f);
    REQUIRE(animator.Play("run"));
    animator.Update(0.1f);
    REQUIRE(animator.FrameIndex() == 1);

    animator.Stop();
    CHECK_FALSE(animator.b_IsPlaying());

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 1);

    REQUIRE(animator.Play("run"));
    CHECK(animator.b_IsPlaying());
    CHECK(animator.FrameIndex() == 0);
}

TEST_CASE("rw::Animator: switching clips resets time and frame offset")
{
    rw::Animator animator;
    animator.AddClip("idle", 0, 4, 10.0f);
    animator.AddClip("walk", 8, 6, 10.0f);
    REQUIRE(animator.Play("idle"));
    animator.Update(0.05f);
    REQUIRE(animator.FrameTime() == doctest::Approx(0.05f));

    REQUIRE(animator.Play("walk"));
    CHECK(animator.CurrentClip() == "walk");
    CHECK(animator.FrameIndex() == 8);
    CHECK(animator.FrameTime() == doctest::Approx(0.0f));

    animator.Update(0.1f);
    CHECK(animator.FrameIndex() == 9);
}

TEST_CASE("rw::Animator: replaying the active clip keeps progress")
{
    rw::Animator animator;
    animator.AddClip("run", 0, 4, 10.0f);
    REQUIRE(animator.Play("run"));
    animator.Update(0.1f);
    REQUIRE(animator.FrameIndex() == 1);

    CHECK(animator.Play("run"));
    CHECK(animator.FrameIndex() == 1);
    CHECK(animator.b_IsPlaying());
}

TEST_CASE("rw::Animator: playing an unknown clip fails without side effects")
{
    rw::Animator animator;
    animator.AddClip("idle", 0, 2, 10.0f);
    REQUIRE(animator.Play("idle"));

    CHECK_FALSE(animator.Play("missing"));
    CHECK(animator.CurrentClip() == "idle");
    CHECK(animator.b_IsPlaying());
}

TEST_CASE("rw::Animator: zero fps and negative dt do not advance")
{
    rw::Animator paused;
    paused.AddClip("broken", 0, 4, 0.0f);
    REQUIRE(paused.Play("broken"));
    paused.Update(1.0f);
    CHECK(paused.FrameIndex() == 0);

    rw::Animator rewind;
    rewind.AddClip("run", 0, 4, 10.0f);
    REQUIRE(rewind.Play("run"));
    rewind.Update(-1.0f);
    CHECK(rewind.FrameIndex() == 0);
}

TEST_CASE("rw::Animator: a long stall stays within the clip range")
{
    rw::Animator looping;
    looping.AddClip("run", 0, 3, 60.0f);
    REQUIRE(looping.Play("run"));
    looping.Update(1000.0f);
    CHECK(looping.FrameIndex() >= 0);
    CHECK(looping.FrameIndex() <= 2);
    CHECK(looping.b_IsPlaying());

    rw::Animator holding;
    holding.AddClip("die", 0, 3, 60.0f, false);
    REQUIRE(holding.Play("die"));
    holding.Update(1000.0f);
    CHECK(holding.FrameIndex() == 2);
    CHECK_FALSE(holding.b_IsPlaying());
}

TEST_CASE("rw::Animator: frame rect uses the absolute frame index")
{
    rw::Animator animator;
    animator.AddClip("run", 4, 2, 10.0f);
    REQUIRE(animator.Play("run"));

    // An invalid sheet collapses to an empty rectangle; the animator still
    // reports the absolute frame it would have sampled.
    const rw::SpriteSheet sheet;
    const Rectangle rect = animator.FrameRect(sheet);
    CHECK(rect.x == doctest::Approx(0.0f));
    CHECK(rect.y == doctest::Approx(0.0f));
    CHECK(rect.width == doctest::Approx(0.0f));
    CHECK(rect.height == doctest::Approx(0.0f));
    CHECK(animator.FrameIndex() == 4);
}

TEST_CASE("rw::SpriteSheet: invalid sheet is safe")
{
    rw::SpriteSheet sheet;
    CHECK_FALSE(sheet.IsValid());
    CHECK(sheet.Columns() == 0);
    CHECK(sheet.FrameSize().x == doctest::Approx(0.0f));
    CHECK(sheet.FrameSize().y == doctest::Approx(0.0f));
    CHECK_FALSE(sheet.GetTexture().IsValid());

    const Rectangle rect = sheet.FrameRect(0);
    CHECK(rect.width == doctest::Approx(0.0f));
    CHECK(rect.height == doctest::Approx(0.0f));

    CHECK(sheet.FrameRect(-1).width == doctest::Approx(0.0f));
    CHECK(sheet.FrameRect(7).height == doctest::Approx(0.0f));
}

TEST_CASE("rw::SpriteSheet: load rejects invalid frame sizes")
{
    rw::SpriteSheet sheet;
    const Vector2 zero_width{ 0.0f, 16.0f };
    CHECK_FALSE(sheet.Load("", zero_width));
    CHECK_FALSE(sheet.IsValid());
    CHECK(sheet.Columns() == 0);
    CHECK(sheet.FrameSize().x == doctest::Approx(0.0f));

    const Vector2 negative_height{ 16.0f, -1.0f };
    CHECK_FALSE(sheet.Load("", negative_height));
    CHECK_FALSE(sheet.IsValid());
}

TEST_CASE("rw::SpriteSheet: load fails cleanly when the texture cannot load")
{
    // An empty path short-circuits in rw::Texture before raylib is touched, so
    // this stays headless and never needs a real image.
    rw::SpriteSheet sheet;
    const Vector2 frame_size{ 16.0f, 16.0f };
    CHECK_FALSE(sheet.Load("", frame_size));
    CHECK_FALSE(sheet.IsValid());
    CHECK(sheet.Columns() == 0);
    CHECK(sheet.FrameSize().y == doctest::Approx(0.0f));
}
