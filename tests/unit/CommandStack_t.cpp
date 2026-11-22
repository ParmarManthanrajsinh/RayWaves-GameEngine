#include "CommandStack.h"
#include "doctest/doctest.h"

#include <memory>
#include <string_view>

namespace
{
class CountingCommand : public IEditorCommand
{
public:
    CountingCommand(std::string_view name, int& applies, int& reverts)
        : m_Name(name), m_Applies(applies), m_Reverts(reverts)
    {
    }

    std::string_view Name() const override { return m_Name; }

    void Apply() override { ++m_Applies; }

    void Revert() override { ++m_Reverts; }

private:
    std::string_view m_Name;
    int& m_Applies;
    int& m_Reverts;
};

std::unique_ptr<IEditorCommand> MakeCommand(std::string_view name, int& applies, int& reverts)
{
    return std::make_unique<CountingCommand>(name, applies, reverts);
}
} // namespace

TEST_CASE("CommandStack: Push applies once, Undo reverts, Redo reapplies")
{
    CommandStack stack;
    int applies = 0;
    int reverts = 0;

    stack.Push(MakeCommand("Move Entity", applies, reverts));
    CHECK(applies == 1);
    CHECK(reverts == 0);
    CHECK(stack.CanUndo());
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.UndoCount() == 1);
    CHECK(stack.RedoCount() == 0);

    CHECK(stack.Undo());
    CHECK(applies == 1);
    CHECK(reverts == 1);
    CHECK_FALSE(stack.CanUndo());
    CHECK(stack.CanRedo());
    CHECK(stack.UndoCount() == 0);
    CHECK(stack.RedoCount() == 1);

    CHECK(stack.Redo());
    CHECK(applies == 2);
    CHECK(reverts == 1);
    CHECK(stack.CanUndo());
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.UndoCount() == 1);
    CHECK(stack.RedoCount() == 0);
}

TEST_CASE("CommandStack: Record stores an applied command without reapplying")
{
    CommandStack stack;
    int applies = 0;
    int reverts = 0;

    stack.Record(MakeCommand("Edit Inspector", applies, reverts));
    CHECK(applies == 0);
    CHECK(reverts == 0);
    CHECK(stack.CanUndo());
    CHECK(stack.UndoCount() == 1);

    CHECK(stack.Undo());
    CHECK(applies == 0);
    CHECK(reverts == 1);

    CHECK(stack.Redo());
    CHECK(applies == 1);
    CHECK(reverts == 1);
}

TEST_CASE("CommandStack: a new Push or Record clears the redo branch")
{
    CommandStack stack;
    int first_applies = 0;
    int first_reverts = 0;
    int second_applies = 0;
    int second_reverts = 0;

    stack.Push(MakeCommand("First", first_applies, first_reverts));
    REQUIRE(stack.Undo());
    REQUIRE(stack.CanRedo());

    stack.Push(MakeCommand("Second", second_applies, second_reverts));
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.RedoCount() == 0);

    REQUIRE(stack.Undo());
    REQUIRE(stack.CanRedo());

    stack.Record(MakeCommand("Third", second_applies, second_reverts));
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.RedoCount() == 0);
    CHECK(stack.UndoCount() == 1);
    REQUIRE(stack.PeekUndo() != nullptr);
    CHECK(stack.PeekUndo()->Name() == "Third");
}

TEST_CASE("CommandStack: max_undo evicts the oldest command and keeps counts correct")
{
    CommandStack stack(3);
    int applies[5] = {};
    int reverts[5] = {};

    for (int i = 0; i < 5; ++i)
    {
        stack.Push(MakeCommand("Command", applies[i], reverts[i]));
    }

    CHECK(stack.UndoCount() == 3);
    CHECK(applies[0] == 1);
    CHECK(applies[1] == 1);

    // The two oldest commands were evicted, so they can never be reverted.
    CHECK(stack.Undo());
    CHECK(reverts[4] == 1);
    CHECK(stack.Undo());
    CHECK(reverts[3] == 1);
    CHECK(stack.Undo());
    CHECK(reverts[2] == 1);
    CHECK_FALSE(stack.Undo());
    CHECK(reverts[0] == 0);
    CHECK(reverts[1] == 0);

    CHECK(stack.RedoCount() == 3);
    CHECK(stack.Redo());
    CHECK(applies[2] == 2);
    CHECK(stack.Redo());
    CHECK(applies[3] == 2);
    CHECK(stack.Redo());
    CHECK(applies[4] == 2);
    CHECK_FALSE(stack.Redo());
    CHECK(applies[0] == 1);
    CHECK(applies[1] == 1);
}

TEST_CASE("CommandStack: zero max_undo retains no history")
{
    CommandStack stack(0);
    int applies = 0;
    int reverts = 0;

    stack.Push(MakeCommand("Transient", applies, reverts));
    CHECK(applies == 1);
    CHECK(stack.UndoCount() == 0);
    CHECK_FALSE(stack.CanUndo());
    CHECK_FALSE(stack.Undo());
}

TEST_CASE("CommandStack: Undo and Redo on empty stacks return false")
{
    CommandStack stack;

    CHECK_FALSE(stack.CanUndo());
    CHECK_FALSE(stack.CanRedo());
    CHECK_FALSE(stack.Undo());
    CHECK_FALSE(stack.Redo());
    CHECK(stack.UndoCount() == 0);
    CHECK(stack.RedoCount() == 0);
    CHECK(stack.PeekUndo() == nullptr);
    CHECK(stack.PeekRedo() == nullptr);
}

TEST_CASE("CommandStack: Peek exposes the top command names")
{
    CommandStack stack;
    int alpha_applies = 0;
    int alpha_reverts = 0;
    int beta_applies = 0;
    int beta_reverts = 0;

    stack.Push(MakeCommand("Alpha", alpha_applies, alpha_reverts));
    stack.Push(MakeCommand("Beta", beta_applies, beta_reverts));

    REQUIRE(stack.PeekUndo() != nullptr);
    CHECK(stack.PeekUndo()->Name() == "Beta");
    CHECK(stack.PeekRedo() == nullptr);

    REQUIRE(stack.Undo());
    REQUIRE(stack.PeekUndo() != nullptr);
    REQUIRE(stack.PeekRedo() != nullptr);
    CHECK(stack.PeekUndo()->Name() == "Alpha");
    CHECK(stack.PeekRedo()->Name() == "Beta");

    REQUIRE(stack.Redo());
    REQUIRE(stack.PeekUndo() != nullptr);
    CHECK(stack.PeekUndo()->Name() == "Beta");
    CHECK(stack.PeekRedo() == nullptr);
}

TEST_CASE("CommandStack: Clear drops both histories")
{
    CommandStack stack;
    int applies = 0;
    int reverts = 0;

    stack.Push(MakeCommand("Move", applies, reverts));
    stack.Push(MakeCommand("Scale", applies, reverts));
    REQUIRE(stack.Undo());
    REQUIRE(stack.CanRedo());

    stack.Clear();
    CHECK_FALSE(stack.CanUndo());
    CHECK_FALSE(stack.CanRedo());
    CHECK(stack.UndoCount() == 0);
    CHECK(stack.RedoCount() == 0);
    CHECK(stack.PeekUndo() == nullptr);
    CHECK(stack.PeekRedo() == nullptr);
}

TEST_CASE("CommandStack: null commands are ignored")
{
    CommandStack stack;
    int applies = 0;
    int reverts = 0;

    stack.Push(nullptr);
    stack.Record(nullptr);
    CHECK_FALSE(stack.CanUndo());
    CHECK(stack.UndoCount() == 0);

    stack.Push(MakeCommand("Real", applies, reverts));
    CHECK(applies == 1);
    CHECK(stack.UndoCount() == 1);
}
