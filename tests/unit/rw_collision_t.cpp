#include "Engine/rw_collision.h"
#include "doctest/doctest.h"
#include <vector>

TEST_CASE("rw::MoveAndCollide: moves freely through empty solids")
{
    Rectangle body{ 0.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids;

    const rw::MoveResult result = rw::MoveAndCollide(body, { 100.0f, 50.0f }, 0.5f, solids);

    CHECK_FALSE(result.b_HitX);
    CHECK_FALSE(result.b_HitY);
    CHECK_FALSE(result.b_Grounded);
    CHECK(body.x == doctest::Approx(50.0f));
    CHECK(body.y == doctest::Approx(25.0f));
}

TEST_CASE("rw::MoveAndCollide: ignores solids that are out of reach")
{
    Rectangle body{ 0.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 500.0f, 500.0f, 100.0f, 100.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { 100.0f, 100.0f }, 1.0f, solids);

    CHECK_FALSE(result.b_HitX);
    CHECK_FALSE(result.b_HitY);
    CHECK(body.x == doctest::Approx(100.0f));
    CHECK(body.y == doctest::Approx(100.0f));
}

TEST_CASE("rw::MoveAndCollide: horizontal hit clamps the body to the solid edge")
{
    Rectangle body{ 0.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 50.0f, 0.0f, 100.0f, 100.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { 100.0f, 0.0f }, 1.0f, solids);

    CHECK(result.b_HitX);
    CHECK_FALSE(result.b_HitY);
    CHECK_FALSE(result.b_Grounded);
    CHECK(body.x == doctest::Approx(18.0f)); // solid.x - body.width
    CHECK(body.x + body.width == doctest::Approx(solids.front().x));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));

    // The input solid list must not be mutated.
    REQUIRE(solids.size() == 1);
    CHECK(solids.front().x == doctest::Approx(50.0f));
    CHECK(solids.front().width == doctest::Approx(100.0f));
}

TEST_CASE("rw::MoveAndCollide: horizontal hit from the right clamps to the far edge")
{
    Rectangle body{ 200.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 50.0f, 0.0f, 100.0f, 100.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { -100.0f, 0.0f }, 1.0f, solids);

    CHECK(result.b_HitX);
    CHECK_FALSE(result.b_Grounded);
    CHECK(body.x == doctest::Approx(solids.front().x + solids.front().width));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));
}

TEST_CASE("rw::MoveAndCollide: landing on top reports grounded")
{
    Rectangle body{ 0.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 0.0f, 50.0f, 200.0f, 50.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { 0.0f, 100.0f }, 1.0f, solids);

    CHECK_FALSE(result.b_HitX);
    CHECK(result.b_HitY);
    CHECK(result.b_Grounded);
    CHECK(body.y == doctest::Approx(18.0f)); // solid.y - body.height
    CHECK(body.y + body.height == doctest::Approx(solids.front().y));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));
}

TEST_CASE("rw::MoveAndCollide: ceiling hit stops upward movement without grounding")
{
    Rectangle body{ 0.0f, 200.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 0.0f, 100.0f, 200.0f, 50.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { 0.0f, -200.0f }, 1.0f, solids);

    CHECK(result.b_HitY);
    CHECK_FALSE(result.b_HitX);
    CHECK_FALSE(result.b_Grounded);
    CHECK(body.y == doctest::Approx(solids.front().y + solids.front().height));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));
}

TEST_CASE("rw::MoveAndCollide: zero delta time leaves the body untouched")
{
    Rectangle body{ 10.0f, 10.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 0.0f, 0.0f, 100.0f, 100.0f } };

    const rw::MoveResult result = rw::MoveAndCollide(body, { 100.0f, 100.0f }, 0.0f, solids);

    CHECK_FALSE(result.b_HitX);
    CHECK_FALSE(result.b_HitY);
    CHECK_FALSE(result.b_Grounded);
    CHECK(body.x == doctest::Approx(10.0f));
    CHECK(body.y == doctest::Approx(10.0f));
}

TEST_CASE("rw::MoveAndCollide: body lands exactly on a solid top and stays there")
{
    Rectangle body{ 10.0f, 0.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 0.0f, 100.0f, 200.0f, 20.0f } };

    const rw::MoveResult landing = rw::MoveAndCollide(body, { 0.0f, 100.0f }, 1.0f, solids);
    CHECK(landing.b_Grounded);
    CHECK(body.y == doctest::Approx(68.0f)); // 100 - 32
    CHECK(body.y + body.height == doctest::Approx(solids.front().y));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));

    // Resting on the surface keeps the body on top instead of sinking into it.
    const rw::MoveResult resting = rw::MoveAndCollide(body, { 0.0f, 100.0f }, 0.5f, solids);
    CHECK(resting.b_Grounded);
    CHECK(body.y == doctest::Approx(68.0f));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));
}

TEST_CASE("rw::MoveAndCollide: already-overlapping body escapes without reversing")
{
    Rectangle body{ 60.0f, 10.0f, 32.0f, 32.0f };
    const std::vector<Rectangle> solids{ { 50.0f, 0.0f, 100.0f, 50.0f } };
    REQUIRE(CheckCollisionRecs(body, solids.front()));

    // Moving out of the solid must not snap the body back to the left edge.
    const rw::MoveResult escaping = rw::MoveAndCollide(body, { 100.0f, 0.0f }, 1.0f, solids);
    CHECK_FALSE(escaping.b_HitX);
    CHECK_FALSE(escaping.b_Grounded);
    CHECK(body.x == doctest::Approx(160.0f));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));

    // Once clear, the next move is resolved normally instead of staying trapped.
    const rw::MoveResult returning = rw::MoveAndCollide(body, { -100.0f, 0.0f }, 1.0f, solids);
    CHECK(returning.b_HitX);
    CHECK(body.x == doctest::Approx(solids.front().x + solids.front().width));
    CHECK_FALSE(CheckCollisionRecs(body, solids.front()));
}

TEST_CASE("rw::b_Overlaps: reports any intersecting solid")
{
    const Rectangle body{ 0.0f, 0.0f, 32.0f, 32.0f };

    CHECK_FALSE(rw::b_Overlaps(body, {}));
    CHECK_FALSE(rw::b_Overlaps(body, { { 100.0f, 100.0f, 10.0f, 10.0f } }));
    CHECK(rw::b_Overlaps(body, { { 100.0f, 100.0f, 10.0f, 10.0f }, { 16.0f, 16.0f, 10.0f, 10.0f } }));
}
