#pragma once
#include "./Vector.hpp"
#include "./Matrix.hpp"
#include "./Primitive.hpp"

#include <numbers>
#include <iostream>

namespace Hitagi {

template <typename T>
inline const T radians(T angle) {
    return angle / 180.0 * std::numbers::pi;
}

template <typename T, unsigned D>
Vector<T, D> normalize(const Vector<T, D>& v) {
    return v / v.norm();
}

template <typename T>
Vector<T, 3> cross(const Vector<T, 3>& v1, const Vector<T, 3>& v2) {
    Vector<T, 3> result;
    result.x = v1.y * v2.z - v1.z * v2.y;
    result.y = v1.z * v2.x - v1.x * v2.z;
    result.z = v1.x * v2.y - v1.y * v2.x;
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
const T determinant(const Matrix<T, 3>& mat) {
    return mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
           mat[0][1] * (mat[1][0] * mat[2][2] - mat[1][2] * mat[2][0]) +
           mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]);
}
template <typename T>
const T determinant(const Matrix<T, 4> mat) {
    return mat[0][3] * mat[1][2] * mat[2][1] * mat[3][0] - mat[0][2] * mat[1][3] * mat[2][1] * mat[3][0] -
           mat[0][3] * mat[1][1] * mat[2][2] * mat[3][0] + mat[0][1] * mat[1][3] * mat[2][2] * mat[3][0] +
           mat[0][2] * mat[1][1] * mat[2][3] * mat[3][0] - mat[0][1] * mat[1][2] * mat[2][3] * mat[3][0] -
           mat[0][3] * mat[1][2] * mat[2][0] * mat[3][1] + mat[0][2] * mat[1][3] * mat[2][0] * mat[3][1] +
           mat[0][3] * mat[1][0] * mat[2][2] * mat[3][1] - mat[0][0] * mat[1][3] * mat[2][2] * mat[3][1] -
           mat[0][2] * mat[1][0] * mat[2][3] * mat[3][1] + mat[0][0] * mat[1][2] * mat[2][3] * mat[3][1] +
           mat[0][3] * mat[1][1] * mat[2][0] * mat[3][2] - mat[0][1] * mat[1][3] * mat[2][0] * mat[3][2] -
           mat[0][3] * mat[1][0] * mat[2][1] * mat[3][2] + mat[0][0] * mat[1][3] * mat[2][1] * mat[3][2] +
           mat[0][1] * mat[1][0] * mat[2][3] * mat[3][2] - mat[0][0] * mat[1][1] * mat[2][3] * mat[3][2] -
           mat[0][2] * mat[1][1] * mat[2][0] * mat[3][3] + mat[0][1] * mat[1][2] * mat[2][0] * mat[3][3] +
           mat[0][2] * mat[1][0] * mat[2][1] * mat[3][3] - mat[0][0] * mat[1][2] * mat[2][1] * mat[3][3] -
           mat[0][1] * mat[1][0] * mat[2][2] * mat[3][3] + mat[0][0] * mat[1][1] * mat[2][2] * mat[3][3];
};

template <typename T>
Matrix<T, 3> inverse(const Matrix<T, 3>& mat) {
    T det = determinant(mat);
    if (det == 0) {
        std::cerr << "[HitagiMath] Warning: the matrix is singular! Function will return a identity matrix!" << std::endl;
        return Matrix<T, 3>(static_cast<T>(1));
    }
    T inv_det = static_cast<T>(1) / det;

    Matrix<T, 3> res{};
    res[0][0] = (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) * inv_det;
    res[0][1] = (mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2]) * inv_det;
    res[0][2] = (mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1]) * inv_det;
    res[1][0] = (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) * inv_det;
    res[1][1] = (mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0]) * inv_det;
    res[1][2] = (mat[1][0] * mat[0][2] - mat[0][0] * mat[1][2]) * inv_det;
    res[2][0] = (mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1]) * inv_det;
    res[2][1] = (mat[2][0] * mat[0][1] - mat[0][0] * mat[2][1]) * inv_det;
    res[2][2] = (mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1]) * inv_det;
    return res;
}

template <typename T>
Matrix<T, 4> inverse(const Matrix<T, 4>& mat) {
    Matrix<T, 4> res{};
    const T*     m = static_cast<const T*>(mat);

    std::array<T, 16> inv{};
    T                 det;
    // clang-format off
    inv[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8]  =  m[4] * m[9]  * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9]  * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9]  = -m[0] * m[9]  * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] =  m[0] * m[9]  * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2]  =  m[1] * m[6]  * m[15] - m[1] * m[7]  * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]  - m[13] * m[3] * m[6];
    inv[6]  = -m[0] * m[6]  * m[15] + m[0] * m[7]  * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]  + m[12] * m[3] * m[6];
    inv[10] =  m[0] * m[5]  * m[15] - m[0] * m[7]  * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]  - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5]  * m[14] + m[0] * m[6]  * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]  + m[12] * m[2] * m[5];
    inv[3]  = -m[1] * m[6]  * m[11] + m[1] * m[7]  * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9]  * m[2] * m[7]  + m[9]  * m[3] * m[6];
    inv[7]  =  m[0] * m[6]  * m[11] - m[0] * m[7]  * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8]  * m[2] * m[7]  - m[8]  * m[3] * m[6];
    inv[11] = -m[0] * m[5]  * m[11] + m[0] * m[7]  * m[9]  + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]  - m[8]  * m[1] * m[7]  + m[8]  * m[3] * m[5];
    inv[15] =  m[0] * m[5]  * m[10] - m[0] * m[6]  * m[9]  - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]  + m[8]  * m[1] * m[6]  - m[8]  * m[2] * m[5];
    // clang-format on

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        std::cerr << "[HitagiMath] Warning: the matrix is singular! Function will return a identity matrix!" << std::endl;
        return Matrix<T, 4>(static_cast<T>(1));
    }
    T inv_det = static_cast<T>(1) / det;

    for (unsigned i = 0; i < 16; i++) static_cast<T*>(res)[i] = inv[i] * inv_det;
    return res;
}

template <typename T, unsigned D>
void exchange_yz(Matrix<T, D>& matrix) {
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
Matrix<T, 4> rotate_x(const Matrix<T, 4>& mat, const T angle) {
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
Matrix<T, 4> rotate_y(const Matrix<T, 4>& mat, const T angle) {
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
Matrix<T, 4> rotate_z(const Matrix<T, 4>& mat, const T angle) {
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
    if (std::abs(angle) < std::numeric_limits<T>::epsilon() ||
        (std::abs(axis.x) < std::numeric_limits<T>::epsilon() &&
         std::abs(axis.y) < std::numeric_limits<T>::epsilon() &&
         std::abs(axis.z) < std::numeric_limits<T>::epsilon())) {
        return mat;
    }
    auto    normalized_axis = normalize(axis);
    const T c = std::cos(angle), s = std::sin(angle), _1_c = 1.0f - c;
    const T x = normalized_axis.x, y = normalized_axis.y, z = normalized_axis.z;
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

// euler: [rotate about X, then Y and Z]
template <typename T>
Matrix<T, 4> rotate(const Matrix<T, 4>& mat, const vec3f& euler) {
    T c3, c2, c1, s3, s2, s1;
    c1 = std::cos(euler.x);
    c2 = std::cos(euler.y);
    c3 = std::cos(euler.z);

    s1 = std::sin(euler.x);
    s2 = std::sin(euler.y);
    s3 = std::sin(euler.z);

    // clang-format off
    Matrix<T, 4> rotation = {
        {c2*c3, c3*s1*s2 - c1*s3,  s1*s3 + c1*c3*s2, 0.0f},
        {c2*s3, c1*c3 + s1*s2*s3,  c1*s2*s3 - c3*s1, 0.0f},
        { - s2,            c2*s1,             c1*c2, 0.0f},
        { 0.0f,             0.0f,              0.0f, 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4> rotate(const Matrix<T, 4>& mat, const Vector<T, 4>& quatv) {
    auto    normalized_quatv = normalize(quatv);
    const T x = normalized_quatv.x, y = normalized_quatv.y, z = normalized_quatv.z, w = normalized_quatv.w;
    const T _2x2 = 2 * x * x, _2y2 = 2 * y * y, _2z2 = 2 * z * z, _2w2 = 2 * w * w, _2xy = 2 * x * y, _2xz = 2 * x * z,
            _2xw = 2 * x * w, _2yz = 2 * y * z, _2yw = 2 * y * w, _2zw = 2 * z * w;

    // clang-format off
    Matrix<T, 4> rotate_mat = {
        {1 - _2y2 - _2z2, _2xy - _2zw    , _2xz + _2yw    , 0},
        {_2xy + _2zw    , 1 - _2x2 -_2z2 , _2yz - _2xw    , 0},
        {_2xz - _2yw    , _2yz + _2xw    , 1 - _2x2 - _2y2, 0},
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
Matrix<T, 4> perspective_fov(T fov, T width, T height, T near, T far) {
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
    res[2][2] = 2 / (near - far);
    res[0][3] = (right + left) / (left - right);
    res[1][3] = (top + bottom) / (bottom - top);
    res[2][3] = (far + near) / (near - far);
    return res;
}

template <typename T>
Matrix<T, 4> look_at(const Vector<T, 3>& position, const Vector<T, 3>& direction, const Vector<T, 3>& up) {
    Vector<T, 3> direct    = normalize(direction);
    Vector<T, 3> right     = normalize(cross(direct, up));
    Vector<T, 3> camera_up = normalize(cross(right, direct));
    // clang-format off
    Matrix<T, 4> look_at = {
        {   right.x,    right.y,    right.z,    -dot(right, position)},
        {camera_up.x, camera_up.y, camera_up.z, -dot(camera_up, position)},
        { -direct.x,  -direct.y,  -direct.z,    dot(direct, position)},
        {         0,          0,          0,                         1}};
    // clang-format on
    return look_at;
}

template <typename T>
Vector<T, 3> get_translation(const Matrix<T, 4>& mat) {
    return Vector<T, 3>{mat[0][3], mat[1][3], mat[2][3]};
}

template <typename T>
Vector<T, 3> get_scaling(const Matrix<T, 4>& mat) {
    return Vector<T, 3>{
        Vector<T, 3>(mat.col(0).xyz).norm(),
        Vector<T, 3>(mat.col(1).xyz).norm(),
        Vector<T, 3>(mat.col(2).xyz).norm(),
    };
}

// Return translation, rotation (ZYX), scaling
template <typename T>
std::tuple<Vector<T, 3>, Vector<T, 3>, Vector<T, 3>> decompose(const Matrix<T, 4>& transform) {
    Vector<T, 3> translation = get_translation(transform);
    Vector<T, 3> scaling     = get_scaling(transform);
    Vector<T, 3> rotation{};

    if (determinant(transform) < 0) scaling = -scaling;

    Matrix<T, 3> m{
        Vector<T, 3>(transform[0].xyz),
        Vector<T, 3>(transform[1].xyz),
        Vector<T, 3>(transform[2].xyz),
    };

    m[0] = m[0] / scaling,
    m[1] = m[1] / scaling,
    m[2] = m[2] / scaling,

    rotation.y = -std::asin(m[2][0]);

    T c = std::cos(rotation.y);
    if (std::abs(c) > std::numeric_limits<T>::epsilon()) {
        rotation.x = std::atan2(m[2][1], m[2][2]);
        rotation.z = std::atan2(m[1][0], m[0][0]);
    } else {
        rotation.x = static_cast<T>(0);
        rotation.z = std::atan2(-m[1][0], m[0][0]);
    }
    return {translation, rotation, scaling};
}

template <typename T>
std::tuple<Vector<T, 3>, T> quaternion_to_axis_angle(const Quaternion<T>& quat) {
    T angle      = 2 * std::acos(quat.w);
    T inv_factor = static_cast<T>(1) / std::sqrt(static_cast<T>(1) - quat.w * quat.w);

    return {inv_factor * quat.xyz, angle};
}

template <typename T>
Quaternion<T> axis_angle_to_quternion(const Vector<T, 3>& axis, T angle) {
    auto a = normalize(axis);
    return {
        a.x * std::sin(static_cast<T>(0.5) * angle),
        a.y * std::sin(static_cast<T>(0.5) * angle),
        a.z * std::sin(static_cast<T>(0.5) * angle),
        std::cos(static_cast<T>(0.5) * angle),
    };
}

// euler is (ZYX), rotate about x, y, z, sequentially.
template <typename T>
Vector<T, 3> quaternion_to_euler(const Quaternion<T>& quat) {
    T xz = quat.w * quat.y - quat.x * quat.z;
    T x;
    T z = std::atan2(quat.x * quat.y + quat.w * quat.z, static_cast<T>(0.5) - (quat.y * quat.y + quat.z * quat.z));
    T y = std::atan(xz / std::sqrt(static_cast<T>(0.25) - xz * xz));

    if (std::abs(xz) <= static_cast<T>(0.5)) {
        x = std::atan2(quat.y * quat.z + quat.w * quat.x, static_cast<T>(0.5) - (quat.x * quat.x + quat.y * quat.y));
    } else {
        x = static_cast<T>(2) * std::atan2(quat.x, quat.w) + std::copysign(z, xz);
    }
    return {x, y, z};
}

// euler is [x, y, z] (ZYX), rotate about x, y, z, sequentially.
template <typename T>
Quaternion<T> euler_to_quaternion(const Vector<T, 3> euler) {
    T c1 = std::cos(static_cast<T>(0.5) * euler.x),
      c2 = std::cos(static_cast<T>(0.5) * euler.y),
      c3 = std::cos(static_cast<T>(0.5) * euler.z),
      s1 = std::sin(static_cast<T>(0.5) * euler.x),
      s2 = std::sin(static_cast<T>(0.5) * euler.y),
      s3 = std::sin(static_cast<T>(0.5) * euler.z);

    return {
        s1 * c2 * c3 - c1 * s2 * s3,
        c1 * s2 * c3 + s1 * c2 * s3,
        c1 * c2 * s3 - s1 * s2 * c3,
        c1 * c2 * c3 + s1 * s2 * s3,
    };
}

template <typename T, unsigned D1, unsigned D2>
void shrink(Matrix<T, D1>& mat1, const Matrix<T, D2>& mat2) {
    static_assert(D1 < D2, "[Error] Target matrix order must smaller than source matrix order!");

    for (unsigned row = 0; row < D1; row++)
        for (unsigned col = 0; col < D1; col++) mat1[row][col] = mat2[row][col];
}
template <typename T, unsigned D>
Vector<T, D> absolute(const Vector<T, D>& a) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::abs(a[i]);
    return res;
}
template <typename T, unsigned D>
Matrix<T, D> absolute(const Matrix<T, D>& a) {
    Matrix<T, D> res;
    for (unsigned row = 0; row < D; row++)
        for (unsigned col = 0; col < D; col++) res[row][col] = std::abs(a[row][col]);
    return res;
}

template <typename T, unsigned D>
Vector<T, D> max(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b);
    return res;
}

template <typename T, unsigned D>
Vector<T, D> max(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b[i]);
    return res;
}

template <typename T, unsigned D>
Vector<T, D> min(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b);
    return res;
}

template <typename T, unsigned D>
Vector<T, D> min(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b[i]);
    return res;
}

inline const size_t align(size_t x, size_t a) {
    assert(((a - 1) & a) == 0 && "alignment is not a power of two");
    return (x + a - 1) & ~(a - 1);
}

}  // namespace Hitagi