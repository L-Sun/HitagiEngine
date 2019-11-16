#include "ColorSpaceConversion.hpp"
#include <gtest/gtest.h>

using namespace My;

void vector_eq(const vec3& v1, const vec3& v2, double error = 1e-8) {
    for (size_t i = 0; i < 3; i++) {
        EXPECT_NEAR(v1[i], v2[i], error) << "difference at index: " << i;
    }
}

TEST(ColorSpaceConversionTest, RGB2YCbCr) {
    RGB rgb = {64, 35, 17};
    vector_eq(YCbCr(41.619, 114.106658, 143.96362), ConvertRGB2YCbCr(rgb),
              1e-4);
    vector_eq(rgb, ConvertYCbCr2RGB(YCbCr(41.619, 114.106, 143.96)), 1e-2);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}