#pragma once
#include "./Vector.hpp"
#include "./Matrix.hpp"
#include "./Geometry.hpp"

namespace Hitagi {
constexpr double PI = 3.14159265358979323846;

template <typename T>
inline T radians(T angle) {
    return angle / 180.0 * PI;
}

template <typename T, unsigned D>
Vector<T, D> normalize(const Vector<T, D>& v) {
    return v / v.norm();
}

template <typename T>
Vector<T, 3> cross(const Vector<T, 3>& v1, const Vector<T, 3>& v2) {
    Vector<T, 3> result;
    for (unsigned index = 0; index < 3; index++) {
        unsigned index_a = ((index + 1 == 3) ? 0 : index + 1);
        unsigned index_b = ((index == 0) ? 2 : index - 1);
        result[index]    = v1[index_a] * v2[index_b] - v1[index_b] * v2[index_a];
    }
    return result;
}

template <typename T, unsigned D>
Matrix<T, D> transpose(const Matrix<T, D>& mat) {
    Matrix<T, D> result;
    for (unsigned row = 0; row < D; row++)
        for (unsigned col = 0; col < D; col++) result[col][row] = mat[row][col];

    return result;
}

template <typename T>
Matrix<T, 3> inverse(const Matrix<T, 3>& mat) {
    T det = mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
            mat[0][1] * (mat[1][0] * mat[2][2] - mat[1][2] * mat[2][0]) +
            mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]);
    if (det == 0) return Matrix<T, 3>(1);
    T invdet = 1 / det;

    Matrix<T, 3> res;
    res[0][0] = (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) * invdet;
    res[0][1] = (mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2]) * invdet;
    res[0][2] = (mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1]) * invdet;
    res[1][0] = (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) * invdet;
    res[1][1] = (mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0]) * invdet;
    res[1][2] = (mat[1][0] * mat[0][2] - mat[0][0] * mat[1][2]) * invdet;
    res[2][0] = (mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1]) * invdet;
    res[2][1] = (mat[2][0] * mat[0][1] - mat[0][0] * mat[2][1]) * invdet;
    res[2][2] = (mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1]) * invdet;
    return res;
}

template <typename T>
Matrix<T, 4> inverse(const Matrix<T, 4>& mat) {
    Matrix<T, 4> res;
    const T*     m = static_cast<const T*>(mat);

    std::array<T, 16> inv;
    T                 det;
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) return Matrix<T, 4>(1);

    det = 1.0 / det;

    for (unsigned i = 0; i < 16; i++) static_cast<T*>(res)[i] = inv[i] * det;

    return res;
}

template <typename T, unsigned D>
void exchangeYZ(Matrix<T, D>& matrix) {
    std::swap(matrix.data[1], matrix.data[2]);
}

template <typename T>
Matrix<T, 4> translate(const Matrix<T, 4>& mat, const Vector<T, 3>& v) {
    // clang-format off
    Matrix<T, 4> translation = {
        {1, 0, 0, v.x},
        {0, 1, 0, v.y},
        {0, 0, 1, v.z},
        {0, 0, 0, 1  }
    };
    // clang-format on
    return translation * mat;
}
template <typename T>
Matrix<T, 4> rotateX(const Matrix<T, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);
    // clang-format off
    Matrix<T, 4> rotate_x = {
        {1, 0,  0, 0},
        {0, c, -s, 0},
        {0, s,  c, 0}, 
        {0, 0,  0, 1}
    };
    // clang-format on
    return rotate_x * mat;
}
template <typename T>
Matrix<T, 4> rotateY(const Matrix<T, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4> rotate_y = {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}
    };
    // clang-format on
    return rotate_y * mat;
}
template <typename T>
Matrix<T, 4> rotateZ(const Matrix<T, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4> rotate_z = {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}
    };
    // clang-format on

    return rotate_z * mat;
}
template <typename T>
Matrix<T, 4> rotate(const Matrix<T, 4>& mat, const T angle, const Vector<T, 3>& axis) {
    auto    _axis = normalize(axis);
    const T c = std::cos(angle), s = std::sin(angle), _1_c = 1.0f - c;
    const T x = _axis.x, y = _axis.y, z = _axis.z;
    // clang-format off
    Matrix<T, 4> rotation = {
        {c + x * x * _1_c    , x * y * _1_c - z * s, x * z * _1_c + y * s, 0.0f},
        {x * y * _1_c + z * s, c + y * y * _1_c    , y * z * _1_c - x * s, 0.0f},
        {x * z * _1_c - y * s, y * z * _1_c + x * s, c + z * z * _1_c,     0.0f},
        {0.0f,                 0.0f,                 0.0f,                 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4> rotate(const Matrix<T, 4>& mat, const T yaw, const T pitch, const T roll) {
    T cYaw, cPitch, cRoll, sYaw, sPitch, sRoll;
    cYaw   = std::cos(yaw);
    cPitch = std::cos(pitch);
    cRoll  = std::cos(roll);

    sYaw   = std::sin(yaw);
    sPitch = std::sin(pitch);
    sRoll  = std::sin(roll);

    // clang-format off
    Matrix<T, 4> rotation = {
        {(cRoll * cYaw) + (sRoll * sPitch * sYaw) , (-sRoll * cYaw) + (cRoll * sPitch * sYaw) , (cPitch * sYaw), 0.0f},
        {(sRoll * cPitch)                         , (cRoll * cPitch)                          , -sPitch        , 0.0f},
        {(cRoll * -sYaw) + (sRoll * sPitch * cYaw), (sRoll * sYaw) + (cRoll * sPitch * cYaw)  , (cPitch * cYaw), 0.0f},
        {0.0f                                     , 0.0f                                      , 0.0f           , 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4> rotate(const Matrix<T, 4>& mat, const Vector<T, 4>& quatv) {
    auto    _quatv = normalize(quatv);
    const T a = _quatv.x, b = _quatv.y, c = _quatv.z, d = _quatv.w;
    const T _2a2 = 2 * a * a, _2b2 = 2 * b * b, _2c2 = 2 * c * c, _2d2 = 2 * d * d, _2ab = 2 * a * b, _2ac = 2 * a * c,
            _2ad = 2 * a * d, _2bc = 2 * b * c, _2bd = 2 * b * d, _2cd = 2 * c * d;

    // clang-format off
    Matrix<T, 4> rotate_mat = {
        {1 - _2c2 - _2d2, _2bc - _2ad    , _2ac + _2bd    , 0},
        {_2bc + _2ad    , 1 - _2b2 -_2d2 , _2cd - _2ab    , 0},
        {_2bd - _2ac    , _2ab + _2cd    , 1 - _2b2 - _2c2, 0},
        {0              , 0              , 0              , 1}
    };
    // clang-format on
    return rotate_mat * mat;
}
template <typename T>
Matrix<T, 4> scale(const Matrix<T, 4>& mat, T s) {
    auto res = mat;
    for (unsigned i = 0; i < 4; i++) {
        res[0][i] *= s;
        res[1][i] *= s;
        res[2][i] *= s;
    }
    return res;
}
template <typename T>
Matrix<T, 4> scale(const Matrix<T, 4>& mat, const Vector<T, 3>& v) {
    auto res = mat;
    for (unsigned i = 0; i < 4; i++) {
        res[0][i] *= v.x;
        res[1][i] *= v.y;
        res[2][i] *= v.z;
    }
    return res;
}

template <typename T>
Matrix<T, 4> perspectiveFov(T fov, T width, T height, T near, T far) {
    Matrix<T, 4> res(0);

    const T h   = std::tan(0.5 * fov);
    const T w   = h * width / height;
    const T nmf = near - far;

    res[0][0] = 1 / w;
    res[1][1] = 1 / h;
    res[2][2] = (near + far) / nmf;
    res[3][2] = -1;
    res[2][3] = 2 * near * far / nmf;
    return res;
}
template <typename T>
Matrix<T, 4> perspective(T fov, T aspect, T near, T far) {
    Matrix<T, 4> res(0);

    const T h   = std::tan(0.5 * fov);
    const T w   = h * aspect;
    const T nmf = near - far;

    res[0][0] = 1 / w;
    res[1][1] = 1 / h;
    res[2][2] = (near + far) / nmf;
    res[3][2] = -1;
    res[2][3] = 2 * near * far / nmf;
    return res;
}
template <typename T>
Matrix<T, 4> ortho(T left, T right, T bottom, T top, T near, T far) {
    Matrix<T, 4> res(1);
    res[0][0] = 2 / (right - left);
    res[1][1] = 2 / (top - bottom);
    res[2][2] = -2 / (far - near);
    res[0][3] = -(right + left) / (right - left);
    res[1][3] = -(top + bottom) / (top - bottom);
    res[2][3] = -(far + near) / (far - near);
    return res;
}

template <typename T>
Matrix<T, 4> lookAt(const Vector<T, 3>& position, const Vector<T, 3>& direction, const Vector<T, 3>& up) {
    Vector<T, 3> direct   = normalize(direction);
    Vector<T, 3> right    = normalize(cross(direct, up));
    Vector<T, 3> cameraUp = normalize(cross(right, direct));
    // clang-format off
    Matrix<T, 4> look_at = {
        {   right.x,    right.y,    right.z,    -dot(right, position)},
        {cameraUp.x, cameraUp.y, cameraUp.z, -dot(cameraUp, position)},
        { -direct.x,  -direct.y,  -direct.z,    dot(direct, position)},
        {         0,          0,          0,                         1}};
    // clang-format on
    return look_at;
}

template <typename T>
const Vector<T, 3> GetOrigin(const Matrix<T, 3>& mat) {
    return Vector<T, 3>{mat[0][3], mat[1][3], mat[2][3]};
}
template <typename T>
const Vector<T, 3> GetOrigin(const Matrix<T, 4>& mat) {
    return Vector<T, 3>{mat[0][3], mat[1][3], mat[2][3]};
}

template <typename T, unsigned D1, unsigned D2>
void Shrink(Matrix<T, D1>& mat1, const Matrix<T, D2>& mat2) {
    static_assert(D1 < D2, "[Error] Target matrix order must smaller than source matrix order!");

    for (unsigned row = 0; row < D1; row++)
        for (unsigned col = 0; col < D1; col++) mat1[row][col] = mat2[row][col];
}
template <typename T, unsigned D>
const Vector<T, D> Absolute(const Vector<T, D>& a) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::abs(a[i]);
    return res;
}
template <typename T, unsigned D>
const Matrix<T, D> Absolute(const Matrix<T, D>& a) {
    Matrix<T, D> res;
    for (unsigned row = 0; row < D; row++)
        for (unsigned col = 0; col < D; col++) res[row][col] = std::abs(a[row][col]);
    return res;
}

template <typename T, unsigned D>
const Vector<T, D> Max(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b);
    return res;
}

template <typename T, unsigned D>
const Vector<T, D> Max(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b[i]);
    return res;
}

template <typename T, unsigned D>
const Vector<T, D> Min(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b);
    return res;
}

template <typename T, unsigned D>
const Vector<T, D> Min(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b[i]);
    return res;
}

inline size_t align(size_t x, size_t a) {
    assert(((a - 1) & a) == 0 && "alignment is not a power of two");
    return (x + a - 1) & ~(a - 1);
}

}  // namespace Hitagi