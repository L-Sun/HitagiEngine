#pragma once
#if defined(Use_Eigen)
#include "Eigen/Dense"
#else
#include "geommath_ispc.hpp"
#endif  // Use_Eigen

namespace My {
constexpr double PI = 3.14159265358979323846;

using vec2f = Vector<float, 2>;
using vec3f = Vector<float, 3>;
using vec4f = Vector<float, 4>;
using quatf = Vector<float, 4>;
using mat3f = Matrix<float, 3, 3>;
using mat4f = Matrix<float, 4, 4>;

using R8G8B8A8Unorm = Vector<uint8_t, 4>;

using vec2d = Vector<double, 2>;
using vec3d = Vector<double, 3>;
using vec4d = Vector<double, 4>;
using quatd = Vector<double, 4>;
using mat3d = Matrix<double, 3, 3>;
using mat4d = Matrix<double, 4, 4>;

template <typename T>
inline T radians(T angle) {
    return angle / 180.0 * PI;
}

template <typename T, int D>
Vector<T, D> normalize(const Vector<T, D>& v) {
    Vector<T, D> res = v;
    ispc::Normalize(res, D);
    return res;
}

template <typename T>
Vector<T, 3> cross(const Vector<T, 3>& v1, const Vector<T, 3>& v2) {
    Vector<T, 3> result;
    ispc::CrossProduct(v1, v2, result);
    return result;
}

template <typename T, int N>
Matrix<T, N, N> inverse(const Matrix<T, N, N>& mat) {
    Matrix<T, N, N> res;
    bool            success = false;
    if (N == 4) {
        res     = mat;
        success = ispc::InverseMatrix4X4f(res);
    } else {
        success = ispc::Inverse(mat, res, N);
    }
    if (!success) {
        res = Matrix<T, N, N>(1.0f);
        std::cout << "matrix is singular" << std::endl;
    }
    return res;
}

template <typename T, int ROWS, int COLS>
void exchangeYZ(Matrix<T, ROWS, COLS>& matrix) {
    std::swap(matrix.data[1], matrix.data[2]);
}

template <typename T>
Matrix<T, 4, 4> translate(const Matrix<T, 4, 4>& mat, const Vector<T, 3>& v) {
    // clang-format off
    Matrix<T, 4, 4> translation = {
        {1, 0, 0, v.x},
        {0, 1, 0, v.y},
        {0, 0, 1, v.z},
        {0, 0, 0, 1  }
    };
    // clang-format on
    return translation * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateX(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);
    // clang-format off
    Matrix<T, 4, 4> rotate_x = {
        {1, 0,  0, 0},
        {0, c, -s, 0},
        {0, s,  c, 0}, 
        {0, 0,  0, 1}
    };
    // clang-format on
    return rotate_x * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateY(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4, 4> rotate_y = {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}
    };
    // clang-format on
    return rotate_y * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateZ(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4, 4> rotate_z = {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}
    };
    // clang-format on

    return rotate_z * mat;
}
template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const T angle,
                       const Vector<T, 3>& axis) {
    auto    _axis = normalize(axis);
    const T c = std::cos(angle), s = std::sin(angle), _1_c = 1.0f - c;
    const T x = _axis.x, y = _axis.y, z = _axis.z;
    // clang-format off
    Matrix<T, 4, 4> rotation = {
        {c + x * x * _1_c    , x * y * _1_c - z * s, x * z * _1_c + y * s, 0.0f},
        {x * y * _1_c + z * s, c + y * y * _1_c    , y * z * _1_c - x * s, 0.0f},
        {x * z * _1_c - y * s, y * z * _1_c + x * s, c + z * z * _1_c,     0.0f},
        {0.0f,                 0.0f,                 0.0f,                 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const T yaw, const T pitch,
                       const T roll) {
    T cYaw, cPitch, cRoll, sYaw, sPitch, sRoll;
    cYaw   = std::cos(yaw);
    cPitch = std::cos(pitch);
    cRoll  = std::cos(roll);

    sYaw   = std::sin(yaw);
    sPitch = std::sin(pitch);
    sRoll  = std::sin(roll);

    // clang-format off
    Matrix<T, 4, 4> rotation = {
        {(cRoll * cYaw) + (sRoll * sPitch * sYaw) , (-sRoll * cYaw) + (cRoll * sPitch * sYaw) , (cPitch * sYaw), 0.0f},
        {(sRoll * cPitch)                         , (cRoll * cPitch)                          , -sPitch        , 0.0f},
        {(cRoll * -sYaw) + (sRoll * sPitch * cYaw), (sRoll * sYaw) + (cRoll * sPitch * cYaw)  , (cPitch * cYaw), 0.0f},
        {0.0f                                     , 0.0f                                      , 0.0f           , 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const Vector<T, 4>& quatv) {
    quatv     = normalize(quatv);
    const T a = quatv.x, b = quatv.y, c = quatv.z, d = quatv.w;
    const T _2a2 = 2 * a * a, _2b2 = 2 * b * b, _2c2 = 2 * c * c,
            _2ab = 2 * a * b, _2ac = 2 * a * c, _2ad = 2 * a * d,
            _2bc = 2 * b * c, _2bd = 2 * b * d, _2cd = 2 * c * d;

    // clang-format off
    Matrix<T, 4, 4> rotate_mat = {
        {1 - _2b2 - _2c2, _2ab + _2cd    , _2ac - _2bd    , 0},
        {_2ab - _2cd    , 1 - _2a2, -_2c2, _2bc + _2ad    , 0},
        {_2ac + _2bd    , _2bc - _2ad    , 1 - _2a2 - _2b2, 0},
        {0              , 0              , 0              , 1}
    };
    // clang-format on
    return rotate_mat * mat;
}
template <typename T>
Matrix<T, 4, 4> scale(const Matrix<T, 4, 4>& mat, const Vector<T, 3>& v) {
    auto res = mat;
    for (size_t i = 0; i < 4; i++) {
        res(0, i) *= v.x;
        res(1, i) *= v.y;
        res(2, i) *= v.z;
    }
    return res;
}

template <typename T>
Matrix<T, 4, 4> perspectiveFov(T fov, T width, T height, T near, T far) {
    Matrix<T, 4, 4> res(static_cast<T>(0));

    const T h   = std::tan(static_cast<T>(0.5) * fov);
    const T w   = h * height / width;
    const T nmf = near - far;

    res(0, 0) = 1 / w;
    res(1, 1) = 1 / h;
    res(2, 2) = (near + far) / nmf;
    res(3, 2) = static_cast<T>(-1);
    res(2, 3) = static_cast<T>(2) * near * far / nmf;
    return res;
}
template <typename T>
Matrix<T, 4, 4> perspective(T fov, T aspect, T near, T far) {
    Matrix<T, 4, 4> res(static_cast<T>(0));

    const T h   = std::tan(static_cast<T>(0.5) * fov);
    const T w   = h * aspect;
    const T nmf = near - far;

    res(0, 0) = static_cast<T>(1) / w;
    res(1, 1) = static_cast<T>(1) / h;
    res(2, 2) = (near + far) / nmf;
    res(3, 2) = -static_cast<T>(1);
    res(2, 3) = static_cast<T>(2) * near * far / nmf;
    return res;
}

template <typename T>
Matrix<T, 4, 4> lookAt(const Vector<T, 3>& position, const Vector<T, 3>& target,
                       const Vector<T, 3>& up) {
    Vector<T, 3> direct   = normalize(target - position);
    Vector<T, 3> right    = normalize(cross(direct, up));
    Vector<T, 3> cameraUp = normalize(cross(right, direct));
    // clang-format off
    Matrix<T, 4, 4> look_at = {
        {   right.x,    right.y,    right.z,    -right * position},
        {cameraUp.x, cameraUp.y, cameraUp.z, -cameraUp * position},
        { -direct.x,  -direct.y,  -direct.z,    direct * position},
        {         0,          0,          0,                    1}};
    // clang-format on
    return look_at;
}

template <typename T>
Matrix<T, 8, 8> DCT8x8(const Matrix<T, 8, 8>& pixel_block) {
    Matrix<T, 8, 8> res;
    ispc::DCT(pixel_block, res);
    return res;
}

template <typename T>
Matrix<T, 8, 8> IDCT8x8(const Matrix<T, 8, 8>& pixel_block) {
    Matrix<T, 8, 8> res;
    ispc::IDCT(pixel_block, res);
    return res;
}

template <typename T, int ROWS, int COLS>
const Vector<T, 3> GetOrigin(const Matrix<T, ROWS, COLS>& mat) {
    static_assert(
        ROWS >= 3,
        "[Error] Only 3x3 and above matrix can be passed to this method!");
    static_assert(
        COLS >= 3,
        "[Error] Only 3x3 and above matrix can be passed to this method!");
    return Vector<T, 3>({mat(0, 3), mat(1, 3), mat(2, 3)});
}

template <typename T, int ROWS1, int COLS1, int ROWS2, int COLS2>
void Shrink(Matrix<T, ROWS1, COLS1>&       mat1,
            const Matrix<T, ROWS2, COLS2>& mat2) {
    static_assert(
        ROWS1 < ROWS2,
        "[Error] Target matrix ROWS must smaller than source matrix ROWS!");
    static_assert(
        COLS1 < COLS2,
        "[Error] Target matrix COLS must smaller than source matrix COLS!");

    for (int i = 0; i < COLS1; i++) {
        mat1.data[i] = mat2.data[i];
    }
}

template <typename T, int ROWS, int COLS>
const Matrix<T, ROWS, COLS> Absolute(const Matrix<T, ROWS, COLS>& mat) {
    Matrix<T, ROWS, COLS> res;
    ispc::Absolute(res, mat, ROWS * COLS);
    return res;
}

template <typename T, int D>
const Vector<T, D> Max(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res;
    ispc::Max(res, a, b, D);
    return res;
}

template <typename T, int D>
const Vector<T, D> Min(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res;
    ispc::Min(res, a, b, D);
    return res;
}

template <typename T, int D>
const T Length(const Vector<T, D>& v) {
    return static_cast<T>(std::sqrt(v * v));
}

}  // namespace My