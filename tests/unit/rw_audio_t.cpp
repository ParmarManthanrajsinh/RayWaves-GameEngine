#include "Engine/rw_audio.h"
#include "doctest/doctest.h"
#include <type_traits>
#include <utility>

// These cases never initialize the audio device and never load a real asset:
// the wrappers must stay inert and safe in a headless test binary.

TEST_CASE("rw::Sound: default sound is inert")
{
    rw::Sound sound;

    CHECK_FALSE(sound.IsValid());
    CHECK(sound.Get().frameCount == 0);

    // Every operation is a no-op on an inert handle.
    sound.Play();
    sound.Stop();
    sound.SetVolume(0.5f);
    CHECK_FALSE(sound.IsValid());
}

TEST_CASE("rw::Sound: Load rejects empty and missing paths")
{
    rw::Sound sound;

    CHECK_FALSE(sound.Load(""));
    CHECK_FALSE(sound.IsValid());

    CHECK_FALSE(sound.Load("Assets/definitely_missing.wav"));
    CHECK_FALSE(sound.IsValid());
}

TEST_CASE("rw::Sound: move construction and assignment leave the source inert")
{
    rw::Sound source;
    rw::Sound moved(std::move(source));

    CHECK_FALSE(moved.IsValid());
    CHECK_FALSE(source.IsValid());
    CHECK(source.Get().frameCount == 0);

    rw::Sound target;
    target = std::move(moved);

    CHECK_FALSE(target.IsValid());
    CHECK_FALSE(moved.IsValid());
    CHECK(moved.Get().frameCount == 0);

    static_assert(!std::is_copy_constructible_v<rw::Sound>);
    static_assert(!std::is_copy_assignable_v<rw::Sound>);
    static_assert(std::is_move_constructible_v<rw::Sound>);
    static_assert(std::is_move_assignable_v<rw::Sound>);
}

TEST_CASE("rw::Music: default music is inert")
{
    rw::Music music;

    CHECK_FALSE(music.IsValid());
    CHECK_FALSE(music.b_IsPlaying());
    CHECK(music.Get().ctxData == nullptr);

    // Frame pump and transport controls tolerate a device-less host.
    music.Update();
    music.Play();
    music.Pause();
    music.Resume();
    music.Stop();
    music.SetVolume(0.25f);
    CHECK_FALSE(music.b_IsPlaying());
}

TEST_CASE("rw::Music: Load rejects empty and missing paths")
{
    rw::Music music;

    CHECK_FALSE(music.Load(""));
    CHECK_FALSE(music.IsValid());

    CHECK_FALSE(music.Load("Assets/definitely_missing.ogg"));
    CHECK_FALSE(music.IsValid());
}

TEST_CASE("rw::Music: move construction and assignment leave the source inert")
{
    rw::Music source;
    rw::Music moved(std::move(source));

    CHECK_FALSE(moved.IsValid());
    CHECK_FALSE(source.IsValid());
    CHECK(source.Get().ctxData == nullptr);

    rw::Music target;
    target = std::move(moved);

    CHECK_FALSE(target.IsValid());
    CHECK_FALSE(moved.IsValid());
    CHECK(moved.Get().ctxData == nullptr);

    static_assert(!std::is_copy_constructible_v<rw::Music>);
    static_assert(!std::is_copy_assignable_v<rw::Music>);
    static_assert(std::is_move_constructible_v<rw::Music>);
    static_assert(std::is_move_assignable_v<rw::Music>);
}

TEST_CASE("rw::Font: default font is inert with the default size")
{
    rw::Font font;

    CHECK_FALSE(font.IsValid());
    CHECK(font.Size() == 32);
    CHECK(font.Get().texture.id == 0);

    const Vector2 measured = font.Measure("Hello");
    CHECK(measured.x == doctest::Approx(0.0f));
    CHECK(measured.y == doctest::Approx(0.0f));
}

TEST_CASE("rw::Font: empty and missing paths stay inert")
{
    rw::Font empty_path("", 48);

    CHECK_FALSE(empty_path.IsValid());
    CHECK(empty_path.Size() == 48);
    CHECK(empty_path.Measure("").x == doctest::Approx(0.0f));

    rw::Font missing_path("Assets/definitely_missing.ttf");

    CHECK_FALSE(missing_path.IsValid());
    CHECK(missing_path.Get().texture.id == 0);
    CHECK_FALSE(missing_path.Load("Assets/still_missing.ttf"));
    CHECK_FALSE(missing_path.IsValid());
    CHECK_FALSE(missing_path.Load("Assets/still_missing.ttf", 48));
    CHECK(missing_path.Size() == 48);
}

TEST_CASE("rw::Font: move construction and assignment leave the source inert")
{
    rw::Font source("Assets/definitely_missing.ttf", 24);
    rw::Font moved(std::move(source));

    CHECK_FALSE(moved.IsValid());
    CHECK_FALSE(source.IsValid());
    CHECK(source.Get().texture.id == 0);

    rw::Font target;
    target = std::move(moved);

    CHECK_FALSE(target.IsValid());
    CHECK_FALSE(moved.IsValid());
    CHECK(moved.Get().texture.id == 0);

    static_assert(!std::is_copy_constructible_v<rw::Font>);
    static_assert(!std::is_copy_assignable_v<rw::Font>);
    static_assert(std::is_move_constructible_v<rw::Font>);
    static_assert(std::is_move_assignable_v<rw::Font>);
}
