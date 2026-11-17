#pragma once
#include "Args.h"
#include "CliCommon.h"

namespace Cli
{
int Run(const FArgs& args);
int RunWorker(const FArgs& args);

// Defined in RunWorker.cpp: supervisor + child worker for headless runs.
int RunCommand(const FProjectContext& project, const FArgs& args, bool b_json);
} // namespace Cli
