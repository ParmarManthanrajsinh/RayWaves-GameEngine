#include "TerminalPanel.h"

#include "../GameEditor.h"

void TerminalPanel::Draw(GameEditor* editor)
{
    if (editor == nullptr)
    {
        return;
    }

    editor->GetTerminal().show(ICON_FA_TERMINAL " Terminal", &editor->m_bShowTerminal);
}
