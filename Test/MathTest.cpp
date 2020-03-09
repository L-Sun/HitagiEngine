#include <iostream>
#include "GeomMath.hpp"
#include <gtest/gtest.h>

using namespace Hitagi;
// clang-format off

vec2f v2 = {1, 2};
vec3f v3 = {1, 2, 3};
mat3f m3 = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};
// mat4f m4 = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

template <typename T, int D>
void vector_eq(const Vector<T, D>& v1, const Vector<T, D>& v2) {
    for (size_t i = 0; i < D; i++) {
        EXPECT_NEAR(v1[i], v2[i], 1E-8) << "difference at index: " << i;
    }
}

template <typename T, int ROWS, int COLS>
void matrix_eq(Matrix<T, ROWS, COLS> mat1, Matrix<T, ROWS, COLS> mat2,
               double epsilon = 1E-5) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            EXPECT_NEAR(mat1(i,j), mat2(i,j), epsilon)
                << "difference at index: [" << i << "][" << j << "]";
        }
    }
}

TEST(VectorTest, VectorInit) {
    vector_eq(v2, vec2f(1, 2));
    vector_eq(v3, vec3f(1, 2, 3));
    vector_eq(vec3f(1, 2, 0), vec3f(v2, 0));
    vector_eq(vec4f(v3, 1), vec4f(1, 2, 3, 1));
}
TEST(VectorTest, VectorCopy) {
    vec3f a(1);
    a = v3;
}
TEST(VectorTest, VectorOperator) {
    vector_eq(v3 + 1, vec3f(2, 3, 4));
    vector_eq(1 + v3, vec3f(2, 3, 4));
    vector_eq(v3 + vec3f(1), vec3f(2, 3, 4));
    vector_eq(-v3, vec3f(-1, -2, -3));
    vector_eq(1 - v3, vec3f(0, -1, -2));
    vector_eq(v3 - vec3f(1), vec3f(0, 1, 2));
    vector_eq(v3 * 2, vec3f(2, 4, 6));
    vector_eq(2 * v3, vec3f(2, 4, 6));
    vector_eq(v3 / 2, vec3f(0.5, 1.0, 1.5));
    EXPECT_NEAR(v3 * vec3f(1, 2, 3), 14, 1e-6);
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
    vec3f v3 = {3, 3, 3};
    vector_eq(v3 / (sqrt(27)), normalize(v3));
}
TEST(SwizzleTest, SwizzleTest) {
    vector_eq((vec3f)v3.zyx, vec3f(3, 2, 1));
    vector_eq((vec3f)v3.rgb, v3);
    v3.rgb = {1, 3, 3};
    vector_eq(v3, vec3f(1, 3, 3));
    v3.zyx = {1, 3, 3};
    vector_eq(v3, vec3f(3, 3, 1));
    v3.r = 4;
    vector_eq(v3, vec3f(4, 3, 1));
    vec4f v4;
    vec4f vx4 = vec4f(1, 2, 3, 4);
    v4       = vx4;
    vx4.x    = 3;
    vector_eq((vec3f)v4.rgb, vec3f(1, 2, 3));
    vector_eq(vec3f(1,2,3), vec3f(vec3f(1,2,3).xyz));
    vector_eq(vec3f(2,1,3), vec3f(vec3f(1,2,3).yxz));
}

TEST(MatrixTest, MatInit) {
    mat3f x = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    matrix_eq(m3, x);
    mat3f _m = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3f _i(1);
    matrix_eq(_i, mat3f({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));
}
TEST(MatrixTest, MatAddNum) {
    matrix_eq(m3 + 3, mat3f({4, 4, 4, 4, 4, 4, 4, 4, 4}));
    matrix_eq(3 + m3, mat3f({4, 4, 4, 4, 4, 4, 4, 4, 4}));
    auto _m3 = m3;
    matrix_eq(_m3 += 3, mat3f({4, 4, 4, 4, 4, 4, 4, 4, 4}));
}
TEST(MatrixTest, MatSubNum) {
    matrix_eq(3 - m3, mat3f({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    matrix_eq(m3 - 3, mat3f({-2, -2, -2, -2, -2, -2, -2, -2, -2}));
    matrix_eq(-m3, mat3f({-1, -1, -1, -1, -1, -1, -1, -1, -1}));
    auto _m3 = m3;
    matrix_eq(_m3 -= 3, mat3f({-2, -2, -2, -2, -2, -2, -2, -2, -2}));
}
TEST(MatrixTest, MatMulNum) {
    matrix_eq(m3 * 2, mat3f({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    matrix_eq(2 * m3, mat3f({2, 2, 2, 2, 2, 2, 2, 2, 2}));
    auto _m3 = m3;
    matrix_eq(_m3 *= 2, mat3f({2, 2, 2, 2, 2, 2, 2, 2, 2}));
}
TEST(MatrixTest, MatDivNum) {
    matrix_eq(m3 / 2, mat3f({0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5}));
    auto _m3 = m3;
    matrix_eq(_m3 /= 2, mat3f({0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5}));
}
TEST(MatrixTest, MatMulMat) {
    mat3f l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3f r = {{9, 8, 7}, {6, 5, 4}, {3, 2, 1}};
    matrix_eq(l * r, mat3f({{30, 24, 18}, {84, 69, 54}, {138, 114, 90}}));
    matrix_eq(r * l, mat3f({{90, 114, 138}, {54, 69, 84}, {18, 24, 30}}));
    Matrix<float, 2, 3> x = {{0, 1, 2}, {3, 4, 5}};
    Matrix<float, 3, 4> y = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}};
    Matrix<float, 2, 4> z = {{20, 23, 26, 29}, {56, 68, 80, 92}};
    matrix_eq(x * y, z);
    matrix_eq(l *= r, mat3f({{30, 24, 18}, {84, 69, 54}, {138, 114, 90}}));
}
TEST(MatrixTest, MatMulVec) {
    mat3f                l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    Matrix<float, 3, 4> y = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}};
    vector_eq(l * vec3f(1, 2, 3), vec3f(14, 32, 50));
    vector_eq(vec3f(1, 2, 3) * y, vec4f(32, 38, 44, 50));
    vec3f v = vec3f(1, 2, 3);
    vector_eq(v *= l, vec3f(30, 36, 42));
}

TEST(TransformTest, Translate) {
    mat4f a = {
        {1, 4, 7, 10},
        {2, 5, 8, 11},
        {3, 6, 9, 12},
        {1, 1, 1,  1}
    };
    mat4f b = {
        {2, 5,  8, 11},
        {4, 7, 10, 13},
        {6, 9, 12, 15},
        {1, 1,  1,  1}
    };
    matrix_eq(translate(a, vec3f(1, 2, 3)), b);
}

TEST(TransformTest, Inverse) {
    mat3f a = {{1, 2, -2}, {2, 5, -3}, {3, 7, -4}};
    mat3f b = {{1, -6, 4}, {-1, 2, -1}, {-1, -1, 1}};
    matrix_eq(inverse(a), b);
    mat4f c = {{1, 2, 3, 4}, {5, 2, 1, 7}, {1, 2, 2, 9}, {6, 7, 8, 1}};
    mat4f d = {{0.38461538, 0.30177515, -0.3964497, -0.08284024},
              {-1.92307692, -0.43195266, 1.13609467, 0.49112426},
              {1.38461538, 0.14792899, -0.70414201, -0.23668639},
              {0.07692308, 0.0295858, 0.0591716, -0.04733728}};
    matrix_eq(inverse(c), d);
}

TEST(TransformTest, DCT8x8Test) {
    // clang-format off
    mat8f pixel_block = {
        {-76, -73, -67, -62, -58, -67, -64, -55},
        {-65, -69, -73, -38, -19, -43, -59, -56},
        {-66, -69, -60, -15, 16, -24, -62, -55},
        {-65, -70, -57, -6, 26, -22, -58, -59},
        {-61, -67, -60, -24, -2, -40, -60, -58},
        {-49, -63, -68, -58, -51, -60, -70, -53},
        {-43, -57, -64, -69, -73, -67, -63, -45},
        {-41, -49, -59, -60, -63, -52, -50, -34}
    };
    mat8f pixel_idct_block = {
        {-416, -33, -60,  32,  48, -40,   0,   0},
        {   0, -24, -56,  19,  26,   0,   0,   0},
        { -42,  13,  80, -24, -40,   0,   0,   0},
        { -42,  17,  44, -29,   0,   0,   0,   0},
        {  18,   0,   0,   0,   0,   0,   0,   0},
        {   0,   0,   0,   0,   0,   0,   0,   0},
        {   0,   0,   0,   0,   0,   0,   0,   0},
        {   0,   0,   0,   0,   0,   0,   0,   0}
    };
    mat8f out = {
        {-415.38, -30.19, -61.20,  27.24,  56.12, -20.10, -2.39,  0.46},
        {   4.47, -21.86, -60.76,  10.25,  13.15,  -7.09, -8.54,  4.88},
        { -46.83,   7.37,  77.13, -24.56, -28.91,   9.93,  5.42, -5.65},
        { -48.53,  12.07,  34.10, -14.76, -10.24,   6.30,  1.83,  1.95},
        {  12.12,  -6.55, -13.20,  -3.95,  -1.87,   1.75, -2.79,  3.14},
        {  -7.73,   2.91,   2.38,  -5.94,  -2.38,   0.94,  4.30,  1.85},
        {  -1.03,   0.18,   0.42,  -2.42,  -0.88,  -3.02,  4.12, -0.66},
        {  -0.17,   0.14,  -1.07,  -4.19,  -1.17,  -0.10,  0.50,  1.68},
    };
    mat8f iout = {
        {-66, -63, -71, -68, -56, -65, -68, -46},
        {-71, -73, -72, -46, -20, -41, -66, -57},
        {-70, -78, -68, -17,  20, -14, -61, -63},
        {-63, -73, -62,  -8,  27, -14, -60, -58},
        {-58, -65, -61, -27,  -6, -40, -68, -50},
        {-57, -57, -64, -58, -48, -66, -72, -47},
        {-53, -46, -61, -74, -65, -63, -62, -45},
        {-47, -34, -53, -74, -60, -47, -47, -41}
    };
    // clang-format on
    matrix_eq(DCT8x8(pixel_block), out, 0.1);
    matrix_eq(IDCT8x8(pixel_idct_block), iout, 0.5);
}
int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}