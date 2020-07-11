#include <iostream>
#include "HitagiMath.hpp"
#include <gtest/gtest.h>

using namespace Hitagi;

template <typename T, unsigned D>
void vector_eq(const Vector<T, D>& v1, const Vector<T, D>& v2) {
    for (size_t i = 0; i < D; i++) {
        EXPECT_NEAR(v1[i], v2[i], 1E-8) << "difference at index: " << i;
    }
}

template <typename T, unsigned D>
void matrix_eq(const Matrix<T, D>& mat1, const Matrix<T, D>& mat2, double epsilon = 1E-5) {
    for (int i = 0; i < D; i++) {
        for (int j = 0; j < D; j++) {
            EXPECT_NEAR(mat1[i][j], mat2[i][j], epsilon) << "difference at index: [" << i << "][" << j << "]";
        }
    }
}

TEST(VectorTest, VectorInit) {
    vec2f v2(1, 2);
    vec3f v3(1, 2, 3);
    vector_eq(vec3f(1, 2, 0), vec3f(v2, 0));
    vector_eq(vec4f(v3, 1), vec4f(1, 2, 3, 1));

    vector_eq(vec4f(1, 2, 3, 4), vec4f{1, 2, 3, 4});
    vector_eq(vec4f(1), vec4f(1));
    Vector<float, 6> v6({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});
}
TEST(VectorTest, VectorCopy) {
    vec3f v3(1);
    vec3f _v3 = v3;
}
TEST(VectorTest, VectorOperator) {
    vec3d v3(1, 2, 3);
    vector_eq(v3 + vec3d(1), vec3d(2, 3, 4));
    vector_eq(-v3, vec3d(-1, -2, -3));
    vector_eq(v3 - vec3d(1), vec3d(0, 1, 2));
    vector_eq(v3 * 2, vec3d(2, 4, 6));
    vector_eq(2.0 * v3, vec3d(2, 4, 6));
    vector_eq(v3 / 2, vec3d(0.5, 1.0, 1.5));
}
TEST(VectorTest, VectorAssignmentOperator) {
    vec3f v3(1, 2, 3);
    auto  _v3 = v3;
    vector_eq(_v3 += v3, 2.0f * v3);
    vector_eq(_v3 -= v3, v3);
    vector_eq(_v3 *= 3, v3 * 3);
    vector_eq(_v3 /= 3, v3);
}
TEST(VectorTest, VectorNormalize) {
    vec3f v3 = {3, 3, 3};
    vector_eq(v3 / (sqrt(27)), normalize(v3));
}
TEST(VectorTest, VectorDotProduct) {
    EXPECT_NEAR(dot(vec3f{1, 2, 3}, vec3f{4, 5, 6}), 32, 1E-6);
    EXPECT_NEAR(dot(vec3f{0, 0, 1}, vec3f{4, 5, 6}), 6, 1E-6);
    EXPECT_NEAR(dot(vec3f{0, 0, 0}, vec3f{4, 5, 6}), 0, 1E-6);
    EXPECT_NEAR(dot(vec3f{0, 0, 0}, vec3f{0, 0, 0}), 0, 1E-6);
}
TEST(VectorTest, VectorCrossProduct) {
    vector_eq(cross(vec3f{12, 14, 62}, vec3f{34, 58, 55}), vec3f{-2826, 1448, 220});
}
TEST(SwizzleTest, SwizzleTest) {
    vec3f v3(1, 2, 3);
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
    v4        = vx4;
    vx4.x     = 3;
    vector_eq((vec3f)v4.rgb, vec3f(1, 2, 3));
    vector_eq(vec3f(1, 2, 3), vec3f(vec3f(1, 2, 3).xyz));
    vector_eq(vec3f(2, 1, 3), vec3f(vec3f(1, 2, 3).yxz));
}

TEST(MatrixTest, MatInit) {
    mat3f _i(1);
    matrix_eq(_i, mat3f({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));

    mat3f m3 = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
}

TEST(MatrixTest, ScalarProduct) {
    mat3f m3 = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};

    matrix_eq(m3 * 2, mat3f{{2, 2, 2}, {2, 2, 2}, {2, 2, 2}});
    matrix_eq(2 * m3, mat3f{{2, 2, 2}, {2, 2, 2}, {2, 2, 2}});
    matrix_eq(m3 *= 2, mat3f{{2, 2, 2}, {2, 2, 2}, {2, 2, 2}});
    matrix_eq(m3 / 2, mat3f{{1, 1, 1}, {1, 1, 1}, {1, 1, 1}});
    matrix_eq(m3 /= 2, mat3f{{1, 1, 1}, {1, 1, 1}, {1, 1, 1}});
}
TEST(MatrixTest, MatAddSubtract) {
    mat3f m1{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    matrix_eq(m1 + m1, 2 * m1);
    matrix_eq(m1 - m1, mat3f(0));
}
TEST(MatrixTest, MatProducts) {
    mat3f l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3f r = {{9, 8, 7}, {6, 5, 4}, {3, 2, 1}};
    matrix_eq(l * r, mat3f{{30, 24, 18}, {84, 69, 54}, {138, 114, 90}});
    matrix_eq(r * l, mat3f{{90, 114, 138}, {54, 69, 84}, {18, 24, 30}});
}
TEST(MatrixTest, VecProducts) {
    mat3f l = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    vector_eq(l * vec3f(1, 2, 3), vec3f(14, 32, 50));
}

TEST(TransformTest, TranslateTest) {
    mat4f a = {{1, 4, 7, 10}, {2, 5, 8, 11}, {3, 6, 9, 12}, {1, 1, 1, 1}};
    mat4f b = {{2, 5, 8, 11}, {4, 7, 10, 13}, {6, 9, 12, 15}, {1, 1, 1, 1}};
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

TEST(TransformTest, InverseSingularMatrix) {
    mat3f a = {{1, 2, 3}, {1, 2, 3}, {3, 7, -4}};
    matrix_eq(inverse(a), mat3f(1));
}

TEST(BenchmarkTest, MatrixOperator) {
    mat4f a = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    mat4f b = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    for (size_t i = 0; i < 100000; i++)
        a = a * b;
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}