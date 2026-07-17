#include "doctest/doctest.h"
#include "../Engine/GameState.h"

TEST_CASE("StateBag: float set/get")
{
    StateBag bag;
    CHECK(bag.GetFloat("gravity") == 0.0f);
    bag.SetFloat("gravity", 9.81f);
    CHECK(bag.GetFloat("gravity") == doctest::Approx(9.81f));
    CHECK(bag.GetFloat("nonexistent") == 0.0f);
}

TEST_CASE("StateBag: float with custom default")
{
    StateBag bag;
    CHECK(bag.GetFloat("speed", 100.0f) == doctest::Approx(100.0f));
}

TEST_CASE("StateBag: int set/get")
{
    StateBag bag;
    CHECK(bag.GetInt("score") == 0);
    bag.SetInt("score", 42);
    CHECK(bag.GetInt("score") == 42);
    CHECK(bag.GetInt("nonexistent") == 0);
}

TEST_CASE("StateBag: bool set/get")
{
    StateBag bag;
    CHECK(bag.GetBool("flag") == false);
    bag.SetBool("flag", true);
    CHECK(bag.GetBool("flag") == true);
    bag.SetBool("flag", false);
    CHECK(bag.GetBool("flag") == false);
}

TEST_CASE("StateBag: string set/get")
{
    StateBag bag;
    CHECK(bag.GetString("name") == "");
    bag.SetString("name", "Player1");
    CHECK(bag.GetString("name") == "Player1");
    CHECK(bag.GetString("nonexistent", "default") == "default");
}

TEST_CASE("StateBag: Vector2 set/get")
{
    StateBag bag;
    Vector2 pos = bag.GetVector2("position");
    CHECK(pos.x == doctest::Approx(0.0f));
    CHECK(pos.y == doctest::Approx(0.0f));

    bag.SetVector2("position", {100.0f, 200.0f});
    Vector2 got = bag.GetVector2("position");
    CHECK(got.x == doctest::Approx(100.0f));
    CHECK(got.y == doctest::Approx(200.0f));
}

TEST_CASE("StateBag: overwrite existing key")
{
    StateBag bag;
    bag.SetInt("lives", 3);
    CHECK(bag.GetInt("lives") == 3);
    bag.SetInt("lives", 5);
    CHECK(bag.GetInt("lives") == 5);
}

TEST_CASE("StateBag: Clear removes all data")
{
    StateBag bag;
    bag.SetFloat("f", 1.0f);
    bag.SetInt("i", 2);
    bag.SetBool("b", true);
    bag.SetString("s", "test");
    bag.Clear();
    CHECK(bag.GetFloat("f") == 0.0f);
    CHECK(bag.GetInt("i") == 0);
    CHECK(bag.GetBool("b") == false);
    CHECK(bag.GetString("s") == "");
}
