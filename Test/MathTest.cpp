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
            EXPECT_NEAR(mat1[i][j], mat2[i][j], 1E-8)
                << "difference at index: [" << i << "][" << j << "]";
        }
    }
}

TEST(VectorTest, VectorInit) {
    vector_eq(v2, vec2({1, 2}));
    vector_eq(v3, vec3({1, 2, 3}));
    vector_eq(vec3({1, 2, 0}), vec3(v2));
}
TEST(VectorTest, VectorCopy) {
    vec3 a(1);
    a = v3;
}
TEST(VectorTest, VectorOperator) {
    vector_eq(v3 + 1, vec3(2, 3, 4));
    vector_eq(1 + v3, vec3(2, 3, 4));
    vector_eq(v3 + vec3(1), vec3(2, 3, 4));
    vector_eq(-v3, vec3(-1, -2, -3));
    vector_eq(1 - v3, vec3(0, -1, -2));
    vector_eq(v3 - vec3(1), vec3(0, 1, 2));
    vector_eq(v3 * 2, vec3(2, 4, 6));
    vector_eq(2 * v3, vec3(2, 4, 6));
    vector_eq(v3 / 2, vec3(0.5, 1.0, 1.5));
    EXPECT_NEAR(v3 * vec3(1, 2, 3), 14, 1e-6);
}
TEST(VectorTest, VectorAssignmentOperator) {
    auto _v3 = v3;
    vector_eq(_v3 += 3, v3 + 3);
    vector_eq(_v3 -= 3, v3);
    vector_eq(_v3 += v3, 2 * v3);
    vector_eq(_v3 -= v3, v3);
    vector_eq(_v3 *= 3, v3 * 3);
    vector_eq(_v3 /= 3, v3);
}
TEST(VectorTest, VectorNormalize) {
    vec3 v3 = {3, 3, 3};
    vector_eq(v3 / (sqrt(27)), normalize(v3));
}
TEST(SwizzleTest, SwizzleTest) {
    vector_eq((vec3)v3.zyx, vec3(3, 2, 1));
    vector_eq((vec3)v3.rgb, v3);
    v3.rgb = {1, 3, 3};
    vector_eq(v3, vec3(1, 3, 3));
    v3.zyx = {1, 3, 3};
    vector_eq(v3, vec3(3, 3, 1));
}

TEST(MatrixTest, MatInit) {
    mat3 x = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    matrix_eq(m3, x);
    mat3 _m = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3 _i(1);
    matrix_eq(_i, mat3({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));
}
TEST(MatrixTest, MatAddNum) {
    matrix_eq(m3 + 3, mat3({4, 4, 4, 4, 4, 4, 4, 4, 4}));
    matrix_eq(3 + m3, mat3({4, 4, 4, 4, 4, 4, 4, 4, 4}));
    auto _m3 = m3;
    matrix_eq(_m3 += 3, mat3({4, 4, 4, 4, 4, 4, 4, 4, 4}));
}
TEST(MatrixTest, MatSubNum) {
    matrix_eq(3 - m3, mat3({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    matrix_eq(m3 - 3, mat3({-2, -2, -2, -2, -2, -2, -2, -2, -2}));
    matrix_eq(-m3, mat3({-1, -1, -1, -1, -1, -1, -1, -1, -1}));
    auto _m3 = m3;
    matrix_eq(_m3 -= 3, mat3({-2, -2, -2, -2, -2, -2, -2, -2, -2}));
}
TEST(MatrixTest, MatMulNum) {
    matrix_eq(m3 * 2, mat3({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    matrix_eq(2 * m3, mat3({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    auto _m3 = m3;
    matrix_eq(_m3 *= 2, mat3({2, 2, 2, 2, 2, 2, 2, 2, 2}));
}
TEST(MatrixTest, MatDivNum) {
    matrix_eq(m3 / 2, mat3({0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5}));
    auto _m3 = m3;
    matrix_eq(_m3 /= 2, mat3({0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5}));
}
TEST(MatrixTest, MatMulMat) {
    mat3 l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3 r = {{9, 8, 7}, {6, 5, 4}, {3, 2, 1}};
    matrix_eq(l * r, mat3({{30, 24, 18}, {84, 69, 54}, {138, 114, 90}}));
    matrix_eq(r * l, mat3({{90, 114, 138}, {54, 69, 84}, {18, 24, 30}}));
    Matrix<float, 2, 3> x = {{0, 1, 2}, {3, 4, 5}};
    Matrix<float, 3, 4> y = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}};
    Matrix<float, 2, 4> z = {{20, 23, 26, 29}, {56, 68, 80, 92}};
    matrix_eq(x * y, z);
    matrix_eq(l *= r, mat3({{30, 24, 18}, {84, 69, 54}, {138, 114, 90}}));
}
TEST(MatrixTest, MatMulVec) {
    mat3                l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    Matrix<float, 3, 4> y = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}};
    vector_eq(l * vec3(1, 2, 3), vec3(14, 32, 50));
    vector_eq(vec3(1, 2, 3) * y, vec4(32, 38, 44, 50));
    vec3 v = vec3(1, 2, 3);
    vector_eq(v *= l, vec3(30, 36, 42));
}
int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}