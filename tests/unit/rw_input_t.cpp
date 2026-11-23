#include "Engine/rw_input.h"
#include "doctest/doctest.h"

TEST_CASE("ActionMap binds single and multiple keys")
{
    rw::ActionMap actions;
    CHECK(actions.BindingCount() == 0);
    CHECK_FALSE(actions.b_IsBound("jump"));

    actions.Bind("jump", KEY_SPACE);
    CHECK(actions.b_IsBound("jump"));
    CHECK(actions.BindingCount() == 1);

    actions.Bind("move_left", { KEY_A, KEY_LEFT });
    CHECK(actions.b_IsBound("move_left"));
    CHECK(actions.BindingCount() == 3);

    const rw::ActionMap& read_only = actions;
    CHECK(read_only.b_IsBound("jump"));
    CHECK(read_only.b_IsBound("move_left"));
}

TEST_CASE("ActionMap ignores empty action names")
{
    rw::ActionMap actions;
    actions.Bind("", KEY_SPACE);
    actions.Bind({}, { KEY_A, KEY_B });

    CHECK(actions.BindingCount() == 0);
    CHECK_FALSE(actions.b_IsBound(""));
    CHECK_FALSE(actions.b_Down(""));
    CHECK_FALSE(actions.b_Pressed(""));
    CHECK_FALSE(actions.b_Released(""));
}

TEST_CASE("ActionMap allows duplicate key bindings")
{
    rw::ActionMap actions;
    actions.Bind("jump", KEY_SPACE);
    actions.Bind("jump", KEY_SPACE);

    CHECK(actions.b_IsBound("jump"));
    CHECK(actions.BindingCount() == 2);

    actions.Unbind("jump");
    CHECK_FALSE(actions.b_IsBound("jump"));
    CHECK(actions.BindingCount() == 0);
}

TEST_CASE("ActionMap unbinds and clears")
{
    rw::ActionMap actions;
    actions.Bind("jump", KEY_SPACE);
    actions.Bind("crouch", { KEY_LEFT_CONTROL, KEY_C });
    CHECK(actions.BindingCount() == 3);

    actions.Unbind("jump");
    CHECK_FALSE(actions.b_IsBound("jump"));
    CHECK(actions.b_IsBound("crouch"));

    actions.Unbind("missing"); // Unknown actions are a no-op.
    CHECK(actions.BindingCount() == 2);

    actions.Clear();
    CHECK(actions.BindingCount() == 0);
    CHECK_FALSE(actions.b_IsBound("crouch"));
}

TEST_CASE("ActionMap queries are false for unbound actions")
{
    rw::ActionMap actions;
    CHECK_FALSE(actions.b_Down("jump"));
    CHECK_FALSE(actions.b_Pressed("jump"));
    CHECK_FALSE(actions.b_Released("jump"));

    actions.Bind("jump", KEY_SPACE);
    actions.Unbind("jump");
    CHECK_FALSE(actions.b_Down("jump"));
    CHECK_FALSE(actions.b_Pressed("jump"));
    CHECK_FALSE(actions.b_Released("jump"));
}
