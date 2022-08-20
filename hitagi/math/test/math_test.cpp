#include <hitagi/math/transform.hpp>

#include <hitagi/utils/test.hpp>

using namespace hitagi::math;
using namespace hitagi::testing;

TEST(VectorTest, VectorInit) {
    vec2f v2(1, 2);
    vec3f v3(1, 2, 3);
    vector_eq(vec3f(1, 2, 0), vec3f(v2, 0));
    vector_eq(vec4f(v3, 1), vec4f(1, 2, 3, 1));

    vector_eq(vec4f(1, 2, 3, 4), vec4f{1, 2, 3, 4});
    vector_eq(vec4f(1), vec4f(1));
}
TEST(VectorTest, VectorCopy) {
    vec3f v3(1);
    vec3f v4 = v3;
    vector_eq(v3, v4);
}
TEST(VectorTest, VectorOperator) {
    vec3d v3(1, 2, 3);
    vector_eq(v3 + vec3d(1), vec3d(2, 3, 4));
    vector_eq(-v3, vec3d(-1, -2, -3));
    vector_eq(v3 - vec3d(1), vec3d(0, 1, 2));
    vector_eq(v3 * 2, vec3d(2, 4, 6));
    vector_eq(2.0 * v3, vec3d(2, 4, 6));
    vector_eq(v3 / 2, vec3d(0.5, 1.0, 1.5));
    vector_eq(v3 / vec3d(1, 2, 3), vec3d(1, 1, 1));
}
TEST(VectorTest, VectorAssignmentOperator) {
    vec3f v3(1, 2, 3);
    auto  v4 = v3;
    vector_eq(v4 += v3, 2.0f * v3);
    vector_eq(v4 -= v3, v3);
    vector_eq(v4 *= 3, v3 * 3);
    vector_eq(v4 /= 3, v3);
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
    mat3f m1 = mat3f::identity();
    matrix_eq(m1, mat3f({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));

    mat3f m3 = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
}

TEST(MatrixTest, MatAssignment) {
    mat3f m1 = mat3f::identity();
    matrix_eq(m1, mat3f({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));

    mat3f m2 = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    mat3f m3 = m2;
    matrix_eq(m3, m2);
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
    matrix_eq(m1 - m1, mat3f::zero());
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
    vector_eq(translate(vec3f(1, 2, 3)) * vec4f(2, 4, 2, 1), vec4f(3, 6, 5, 1));
    vector_eq(translate(vec3f(1, 2, 3)) * vec4f(2, 4, 2, 0), vec4f(2, 4, 2, 0));
}

TEST(TransformTest, ScaleTest) {
    vector_eq(scale(3.0f) * vec4f(1, 2, 3, 1), vec4f(3, 6, 9, 1));
    vector_eq(scale(vec3f(1.0f, 2.0f, 3.0f)) * vec4f(1, 2, 3, 1), vec4f(1, 4, 9, 1));
}

TEST(TransformTest, RotateTest) {
    vector_eq(rotate_x(deg2rad(90.0f)) * vec4f(1, 0, 0, 1), vec4f(1, 0, 0, 1));
    vector_eq(rotate_y(deg2rad(90.0f)) * vec4f(1, 0, 0, 1), vec4f(0, 0, -1, 1));
    vector_eq(rotate_z(deg2rad(90.0f)) * vec4f(1, 0, 0, 1), vec4f(0, 1, 0, 1));

    vector_eq(rotate(vec3f(0, 0, deg2rad(90.0f))) * vec4f(1, 0, 0, 1), vec4f(0, 1, 0, 1));
    vector_eq(rotate(vec3f(0, deg2rad(180.0f), 0)) * vec4f(1, 0, 0, 1), vec4f(-1, 0, 0, 1));
    vector_eq(rotate(vec3f(deg2rad(90.0f), 0, 0)) * vec4f(1, 0, 0, 1), vec4f(1, 0, 0, 1));

    vector_eq(
        rotate(deg2rad(vec3f(30.0f, 45.0f, 90.0f))) * vec4f(1, 2, 1, 1),
        rotate_z(deg2rad(90.0f)) * rotate_y(deg2rad(45.0f)) * rotate_x(deg2rad(30.0f)) * vec4f(1, 2, 1, 1));

    vector_eq(rotate(deg2rad(90.0f), vec3f(0.0f, 0.0f, 1.0f)) * vec4f(1, 0, 0, 1), vec4f(0, 1, 0, 1));
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
    matrix_eq(inverse(a), mat3f::identity());
}

TEST(TransformTest, DecomposeTest) {
    vec3f translation(1.0f, 1.0f, 1.0f);
    quatf rotation = euler_to_quaternion(deg2rad(vec3f(30.0f, 45.0f, 90.0f)));
    vec3f scaling(1.0f, 2.0f, 3.0f);
    mat4f trans1      = translate(translation) * rotate(rotation) * scale(scaling);
    auto [_t, _r, _s] = decompose(trans1);
    vector_eq(_t, translation);
    vector_eq(_r, rotation);
    vector_eq(_s, scaling);
}

TEST(ConvertTest, AxisAngleToQuaternion) {
    vector_eq(
        axis_angle_to_quternion(vec3f(1, 0, 0), deg2rad(30.0f)),
        quatf(0.258819043, 0, 0, 0.9659258));

    vector_eq(
        axis_angle_to_quternion(vec3f(1, 1, 1), deg2rad(30.0f)),
        quatf(0.149429246, 0.149429246, 0.149429246, 0.9659258));

    vector_eq(
        axis_angle_to_quternion(vec3f(1, 2, 3), deg2rad(64.5f)),
        quatf(0.1426144689, 0.28522893786, 0.42784342169, 0.8457278609));
}

TEST(ConvertTest, QuaternionToEuler) {
    vector_eq(
        quaternion_to_euler(quatf(-0.092296, 0.4304593, 0.5609855, 0.7010574)),
        deg2rad(vec3f(30.0f, 45.0f, 90.0f)));
    vector_eq(
        quaternion_to_euler(quatf(0, 0.7071068, 0.7071068, 0)),
        deg2rad(vec3f(90.0f, 0.0f, 180.0f)));
}

TEST(ConvertTest, EulerToQuaternion) {
    vector_eq(
        euler_to_quaternion(deg2rad(vec3f(30.0f, 45.0f, 90.0f))),
        quatf(-0.092296, 0.4304593, 0.5609855, 0.7010574));
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