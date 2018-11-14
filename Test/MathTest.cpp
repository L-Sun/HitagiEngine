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
        EXPECT_NEAR(v1[i], v2[i], 1E-8);
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

TEST(SwizzleTest, SwizzleTest) {}

TEST(MatrixTest, MatrixInit) { matrix_eq(m3, mat3(1.0f)); }

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}