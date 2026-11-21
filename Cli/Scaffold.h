#pragma once
#include "Args.h"

namespace Cli
{
int TemplatesCommand(bool b_json);
int NewCommand(const FArgs& args, bool b_json);
int NewLevelCommand(const FArgs& args, bool b_json);
} // namespace Cli
