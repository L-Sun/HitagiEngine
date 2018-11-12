#include <iostream>
#include "geommath.hpp"
#include <gtest/gtest.h>

using namespace std;
using namespace My;

TEST(VectorTest, VectorInit) { EXPECT_EQ(vec2(1.0, 2.0)); }

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}