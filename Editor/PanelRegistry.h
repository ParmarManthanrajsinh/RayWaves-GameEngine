#pragma once

#include <memory>
#include <vector>
#include "Panels/IEditorPanel.h"

// Panel registry: core panels are listed in s_CorePanelFactories() order by
// GameEditor; anything extra self-registers from its own translation unit via
// b_RegisterPanel<T>() and is appended after the core panels. Adding a panel
// means new files + one registration line, never an edit to GameEditor.cpp.
using FPanelFactory = std::unique_ptr<IEditorPanel>(*)();

template <typename T>
std::unique_ptr<IEditorPanel> s_fMakePanel()
{
    return std::make_unique<T>();
}

// Extension panels, in link order (order across translation units is
// unspecified — extensions must not depend on each other's position).
inline std::vector<FPanelFactory>& s_ExtensionPanels()
{
    static std::vector<FPanelFactory> s_Factories;
    return s_Factories;
}

template <typename T>
bool b_RegisterPanel()
{
    s_ExtensionPanels().push_back(&s_fMakePanel<T>);
    return true;
}
