#include "Editor/EditorCamera3D.h"
#include "doctest/doctest.h"

TEST_CASE("EditorCamera3D: FromCamera rebuilds the same view")
{
    const Camera3D source{ { 3.0f, 4.0f, 5.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE };

    EditorCamera3D camera;
    camera.FromCamera(source);
    REQUIRE(camera.b_IsValid());

    const Camera3D rebuilt = camera.GetCamera();
    CHECK(rebuilt.position.x == doctest::Approx(source.position.x).epsilon(0.001));
    CHECK(rebuilt.position.y == doctest::Approx(source.position.y).epsilon(0.001));
    CHECK(rebuilt.position.z == doctest::Approx(source.position.z).epsilon(0.001));
    CHECK(rebuilt.target.x == doctest::Approx(source.target.x));
    CHECK(rebuilt.target.y == doctest::Approx(source.target.y));
    CHECK(rebuilt.target.z == doctest::Approx(source.target.z));
    CHECK(rebuilt.fovy == doctest::Approx(source.fovy));
    CHECK(rebuilt.projection == CAMERA_PERSPECTIVE);
}

TEST_CASE("EditorCamera3D: look clamps pitch and keeps position")
{
    EditorCamera3D camera;
    camera.FromCamera(Camera3D{ { 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE });

    const Vector3 start = camera.GetPosition();

    camera.Look(0.0f, 100000.0f);
    CHECK(camera.GetPitch() <= doctest::Approx(85.0f));

    camera.Look(0.0f, -200000.0f);
    CHECK(camera.GetPitch() >= doctest::Approx(-85.0f));

    const Vector3 end = camera.GetPosition();
    CHECK(end.x == doctest::Approx(start.x).epsilon(0.001));
    CHECK(end.y == doctest::Approx(start.y).epsilon(0.001));
    CHECK(end.z == doctest::Approx(start.z).epsilon(0.001));
}

TEST_CASE("EditorCamera3D: move flies along the view")
{
    const Camera3D source{ { 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE };

    EditorCamera3D camera;
    camera.FromCamera(source);

    const Vector3 before = camera.GetPosition();
    camera.Move({ 0.0f, 0.0f, 1.0f }, 1.0f, false);
    const Vector3 after = camera.GetPosition();

    CHECK(after.z < before.z);
    CHECK(after.x == doctest::Approx(before.x));

    camera.Move({ 0.0f, 1.0f, 0.0f }, 1.0f, false);
    CHECK(camera.GetPosition().y > after.y);

    EditorCamera3D normal;
    normal.FromCamera(source);
    normal.Move({ 0.0f, 0.0f, 1.0f }, 1.0f, false);
    const float normal_step = Vector3Length(Vector3Subtract(normal.GetPosition(), before));

    EditorCamera3D fast;
    fast.FromCamera(source);
    fast.Move({ 0.0f, 0.0f, 1.0f }, 1.0f, true);
    const float fast_step = Vector3Length(Vector3Subtract(fast.GetPosition(), before));

    CHECK(fast_step > normal_step * 2.5f);
}

TEST_CASE("EditorCamera3D: dolly clamps distance")
{
    EditorCamera3D camera;
    camera.FromCamera(Camera3D{ { 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE });

    for (int index = 0; index < 100; ++index)
    {
        camera.Dolly(1.0f);
    }
    CHECK(camera.GetDistance() >= doctest::Approx(0.5f));

    for (int index = 0; index < 200; ++index)
    {
        camera.Dolly(-1.0f);
    }
    CHECK(camera.GetDistance() <= doctest::Approx(500.0f));
}

TEST_CASE("EditorCamera3D: focus frames a point and radius")
{
    EditorCamera3D camera;
    camera.FromCamera(Camera3D{ { 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE });

    camera.Focus({ 5.0f, 2.0f, 1.0f }, 3.0f);

    const Vector3 pivot = camera.GetPivot();
    CHECK(pivot.x == doctest::Approx(5.0f));
    CHECK(pivot.y == doctest::Approx(2.0f));
    CHECK(pivot.z == doctest::Approx(1.0f));
    CHECK(camera.GetDistance() > 0.0f);

    const Camera3D framed = camera.GetCamera();
    CHECK(framed.target.x == doctest::Approx(5.0f));
    CHECK(framed.target.y == doctest::Approx(2.0f));
    CHECK(framed.target.z == doctest::Approx(1.0f));
}

TEST_CASE("EditorCamera3D: pan moves the pivot sideways")
{
    EditorCamera3D camera;
    camera.FromCamera(Camera3D{ { 0.0f, 0.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 60.0f,
        CAMERA_PERSPECTIVE });

    camera.Pan(20.0f, 0.0f, 600.0f);
    CHECK(camera.GetPivot().x < 0.0f);
    CHECK(camera.GetPivot().y == doctest::Approx(0.0f));
}

TEST_CASE("EditorCamera3D: invalid camera reports not valid")
{
    EditorCamera3D camera;
    CHECK(!camera.b_IsValid());
    camera.Invalidate();
    CHECK(!camera.b_IsValid());
}
