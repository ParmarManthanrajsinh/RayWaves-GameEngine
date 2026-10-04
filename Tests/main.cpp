#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
int main(int argc, char** argv)
{
    doctest::Context ctx(argc, argv);
    return ctx.run();
}
