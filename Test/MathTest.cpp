#include <iostream>
#include "geommath.hpp"
#include <gtest/gtest.h>

using namespace std;
using namespace My;

vec2 v2 = {1, 2};
vec3 v3 = {1, 2, 3};
mat3 m3 = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};
// mat4 m4 = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

template <typename T, int D>
void vector_eq(const Vector<T, D>& v1, const Vector<T, D>& v2) {
    for (size_t i = 0; i < D; i++) {
        EXPECT_NEAR(v1[i], v2[i], 1E-8) << "difference at index: " << i;
    }
}

template <typename T, int ROWS, int COLS>
void matrix_eq(Matrix<T, ROWS, COLS> mat1, Matrix<T, ROWS, COLS> mat2) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            EXPECT_NEAR(mat1[i][j], mat2[i][j], 1E-8);
        }
    }
}

TEST(VectorTest, VectorInit) {
    vector_eq(v2, vec2({1, 2}));
    vector_eq(v3, vec3({1, 2, 3}));
}
TEST(VectorTest, VectorOperator) {
    vector_eq(v3 + 1, vec3(2, 3, 4));
    vector_eq(1 + v3, vec3(2, 3, 4));
    vector_eq(v3 + vec3(1), vec3(2, 3, 4));
    vector_eq(1 - v3, vec3(0, -1, -2));
    vector_eq(v3 - vec3(1), vec3(0, 1, 2));
    vector_eq(v3 * 2, vec3(2, 4, 6));
    vector_eq(2 * v3, vec3(2, 4, 6));
    vector_eq(v3 / 2, vec3(0.5, 1.0, 1.5));
    EXPECT_NEAR(v3 * vec3(1, 2, 3), 14, 1e-6);
}
TEST(VectorTest, VectorAssigmentOperator) {
    auto _v3 = v3;
    vector_eq(_v3 += 3, v3 + 3);
    vector_eq(_v3 -= 3, v3);
    vector_eq(_v3 += v3, 2 * v3);
    vector_eq(_v3 -= v3, v3);
    vector_eq(_v3 *= 3, v3 * 3);
    vector_eq(_v3 /= 3, v3);
}

void test(Vector<float, 3> x) {}

TEST(SwizzleTest, SwizzleTest) {
    vector_eq((vec3)v3.zyx, vec3(3, 2, 1));
    vector_eq((vec3)v3.rgb, v3);
    v3.rgb = {1, 3, 3};
    vector_eq(v3, vec3(1, 3, 3));
    v3.zyx = {1, 3, 3};
    vector_eq(v3, vec3(3, 3, 1));
}

TEST(MatrixTest, MatrixInit) { matrix_eq(m3, mat3(1.0f)); }

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}