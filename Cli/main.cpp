#include "Args.h"
#include "Commands.h"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    FArgs args;
    std::string parse_error;
    if (!b_ParseArgs(argc, argv, args, parse_error))
    {
        std::cerr << "rwcli: " << parse_error << "\n";
        return 2;
    }

    if (!args.m_Positional.empty() && args.m_Positional[0] == "__run-worker")
    {
        return Cli::RunWorker(args);
    }

    return Cli::Run(args);
}
