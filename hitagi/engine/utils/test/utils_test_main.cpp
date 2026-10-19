#include "interop/gtest_macros.hpp"
import std;
import interop.gtest;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
