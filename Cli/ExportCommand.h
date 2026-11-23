#pragma once
#include "Args.h"
#include "CliCommon.h"

namespace Cli
{
int ExportCommand(const FProjectContext& project, const FArgs& args, bool b_json);
} // namespace Cli
