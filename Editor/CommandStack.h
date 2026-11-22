#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

// A single reversible edit. Commands are owned by CommandStack (never copied),
// so callers construct them in place and hand over ownership.
class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;

    // Shown in menus and tooltips, e.g. "Move Entity", "Edit Inspector".
    virtual std::string_view Name() const = 0;

    // Performs the edit. Push() calls this immediately.
    virtual void Apply() = 0;

    // Restores the state captured before Apply().
    virtual void Revert() = 0;
};

// Linear undo/redo history. Undo reverts the newest applied command and parks it
// on the redo stack; any new edit discards that redo branch. Only the most recent
// max_undo commands are kept; older ones fall off the bottom.
class CommandStack
{
public:
    explicit CommandStack(size_t max_undo = 200);

    // Applies the command, then records it as the newest undo step.
    void Push(std::unique_ptr<IEditorCommand> command);

    // Records a command whose edit has already been applied by the caller.
    void Record(std::unique_ptr<IEditorCommand> command);

    bool Undo();
    bool Redo();

    bool CanUndo() const;
    bool CanRedo() const;

    size_t UndoCount() const;
    size_t RedoCount() const;

    // Top of each stack, or nullptr when that stack is empty.
    const IEditorCommand* PeekUndo() const;
    const IEditorCommand* PeekRedo() const;

    void Clear();

private:
    size_t m_MaxUndo;
    std::vector<std::unique_ptr<IEditorCommand>> m_UndoCommands;
    std::vector<std::unique_ptr<IEditorCommand>> m_RedoCommands;
};
