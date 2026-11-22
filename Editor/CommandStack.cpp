#include "CommandStack.h"

#include <utility>

CommandStack::CommandStack(size_t max_undo) : m_MaxUndo(max_undo)
{
}

void CommandStack::Push(std::unique_ptr<IEditorCommand> command)
{
    if (!command)
    {
        return;
    }

    // Apply before recording so a throwing command leaves the history untouched.
    command->Apply();
    Record(std::move(command));
}

void CommandStack::Record(std::unique_ptr<IEditorCommand> command)
{
    if (!command)
    {
        return;
    }

    m_RedoCommands.clear();
    m_UndoCommands.push_back(std::move(command));

    while (m_UndoCommands.size() > m_MaxUndo)
    {
        m_UndoCommands.erase(m_UndoCommands.begin());
    }
}

bool CommandStack::Undo()
{
    if (m_UndoCommands.empty())
    {
        return false;
    }

    std::unique_ptr<IEditorCommand>& pCommand = m_UndoCommands.back();
    pCommand->Revert();
    m_RedoCommands.push_back(std::move(pCommand));
    m_UndoCommands.pop_back();
    return true;
}

bool CommandStack::Redo()
{
    if (m_RedoCommands.empty())
    {
        return false;
    }

    std::unique_ptr<IEditorCommand>& pCommand = m_RedoCommands.back();
    pCommand->Apply();
    m_UndoCommands.push_back(std::move(pCommand));
    m_RedoCommands.pop_back();
    return true;
}

bool CommandStack::CanUndo() const
{
    return !m_UndoCommands.empty();
}

bool CommandStack::CanRedo() const
{
    return !m_RedoCommands.empty();
}

size_t CommandStack::UndoCount() const
{
    return m_UndoCommands.size();
}

size_t CommandStack::RedoCount() const
{
    return m_RedoCommands.size();
}

const IEditorCommand* CommandStack::PeekUndo() const
{
    return m_UndoCommands.empty() ? nullptr : m_UndoCommands.back().get();
}

const IEditorCommand* CommandStack::PeekRedo() const
{
    return m_RedoCommands.empty() ? nullptr : m_RedoCommands.back().get();
}

void CommandStack::Clear()
{
    m_UndoCommands.clear();
    m_RedoCommands.clear();
}
