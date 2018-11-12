#include <iostream>
#include "geommath.hpp"
#include <gtest/gtest.h>

using namespace std;
using namespace My;

TEST(VectorTest, VectorInit) { EXPECT_EQ(1, 1); }

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}