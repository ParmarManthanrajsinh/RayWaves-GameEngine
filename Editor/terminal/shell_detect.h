#pragma once

#include "pty_session.h"

#include <functional>
#include <string>
#include <vector>

namespace term
{

using FShellExists = std::function<bool(const std::wstring&)>;

// Returns available shells in preference order (pwsh, powershell, cmd).
std::vector<t_ShellSpec> DetectShells(const FShellExists& exists = {});
t_ShellSpec DefaultShell(const FShellExists& exists = {});

} // namespace term
