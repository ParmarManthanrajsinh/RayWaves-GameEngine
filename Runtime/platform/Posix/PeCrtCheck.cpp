#include "PeCrtCheck.h"

// POSIX dynamic linking uses one process-wide C++ runtime supplied by the
// host, so there is no static-vs-dynamic CRT split to detect. The API stays
// available so DllLoader and tests compile unchanged.

std::vector<std::string> GetModuleCrtImports(const char* module_path)
{
    (void)module_path;
    return {};
}

bool b_CrtImportsCompatible(const std::vector<std::string>& exe_imports, const std::vector<std::string>& dll_imports)
{
    (void)exe_imports;
    (void)dll_imports;
    return true;
}
