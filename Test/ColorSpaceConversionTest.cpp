#include "ColorSpaceConversion.hpp"
#include <gtest/gtest.h>

using namespace My;

template <typename T, int D>
void vector_eq(const Vector<T, D>& v1, const Vector<T, D>& v2,
               double error = 1e-8) {
    for (size_t i = 0; i < D; i++) {
        EXPECT_NEAR(v1[i], v2[i], error) << "difference at index: " << i;
    }
}

TEST(ColorSpaceConversionTest, RGB2YCbCr) {
    RGB rgb = {64, 35, 17};
    vector_eq(YCbCr(41.619, -13.8933, 15.9636), ConvertRGB2YCbCr(rgb), 1e-4);
    vector_eq(rgb, ConvertYCbCr2RGB(YCbCr(41.619, -13.8933, 15.9636)), 1e-4);
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}