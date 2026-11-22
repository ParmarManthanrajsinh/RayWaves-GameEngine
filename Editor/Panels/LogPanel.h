#pragma once
#include "IEditorPanel.h"
#include <array>
#include <string>

class LogPanel : public IEditorPanel
{
public:
    LogPanel() = default;
    ~LogPanel() override = default;

    void Draw(GameEditor* editor) override;

private:
    std::string m_Filter;
    std::array<bool, 4> m_LevelEnabled{ true, true, true, true };
};
