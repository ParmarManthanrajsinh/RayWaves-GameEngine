#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
int main(int argc, char** argv)
{
    try {
        doctest::Context ctx(argc, argv);
        return ctx.run();
    } catch (...) {
        return -1;
    }
}
