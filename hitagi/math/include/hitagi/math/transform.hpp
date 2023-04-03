#pragma once
#include <hitagi/math/matrix.hpp>

#include <numbers>

constexpr double operator""_deg(long double angle) {
    return angle / 180.0 * std::numbers::pi;
}

namespace hitagi::math {

template <typename T>
constexpr T deg2rad(T angle) {
    return angle / 180.0 * std::numbers::pi;
}

template <typename T>
constexpr T rad2deg(T radians) {
    return radians * 180.0 * std::numbers::inv_pi;
}

constexpr auto to_hex(const vec3f& v) noexcept -> Vector<std::uint8_t, 3> {
    return {
        static_cast<std::uint8_t>(round(v[0] * 255)),
        static_cast<std::uint8_t>(round(v[1] * 255)),
        static_cast<std::uint8_t>(round(v[2] * 255)),
    };
}

template <typename T>
constexpr auto translate(const Vector<T, 3>& v) noexcept -> Matrix<T, 4> {
    // clang-format off
    return {
        {1, 0, 0, v.x},
        {0, 1, 0, v.y},
        {0, 0, 1, v.z},
        {0, 0, 0, 1  }
    };
    // clang-format on
}

template <typename T>
constexpr auto rotate_x(const T angle) noexcept -> Matrix<T, 4> {
    const T c = std::cos(angle), s = std::sin(angle);
    // clang-format off
    return {
        {1, 0,  0, 0},
        {0, c, -s, 0},
        {0, s,  c, 0}, 
        {0, 0,  0, 1}
    };
    // clang-format on
}
template <typename T>
constexpr auto rotate_y(const T angle) noexcept -> Matrix<T, 4> {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    return {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}
    };
    // clang-format on
}
template <typename T>
constexpr auto rotate_z(const T angle) noexcept -> Matrix<T, 4> {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    return {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}
    };
    // clang-format on
}
template <typename T>
constexpr auto rotate(const T angle, const Vector<T, 3>& axis) noexcept -> Matrix<T, 4> {
    if (std::abs(angle) < std::numeric_limits<T>::epsilon() ||
        (std::abs(axis.x) < std::numeric_limits<T>::epsilon() &&
         std::abs(axis.y) < std::numeric_limits<T>::epsilon() &&
         std::abs(axis.z) < std::numeric_limits<T>::epsilon())) {
        return Matrix<T, 4>::identity();
    }
    auto    normalized_axis = normalize(axis);
    const T c = std::cos(angle), s = std::sin(angle), _1_c = 1.0f - c;
    const T x = normalized_axis.x, y = normalized_axis.y, z = normalized_axis.z;
    // clang-format off
    return {
        {c + x * x * _1_c    , x * y * _1_c - z * s, x * z * _1_c + y * s, 0.0f},
        {x * y * _1_c + z * s, c + y * y * _1_c    , y * z * _1_c - x * s, 0.0f},
        {x * z * _1_c - y * s, y * z * _1_c + x * s, c + z * z * _1_c,     0.0f},
        {0.0f,                 0.0f,                 0.0f,                 1.0f}
    };
    // clang-format on
}

// euler: [rotate about X, then Y and Z]
template <typename T>
constexpr auto rotate(const Vector<T, 3>& euler) noexcept -> Matrix<T, 4> {
    T c3, c2, c1, s3, s2, s1;
    c1 = std::cos(euler.x);
    c2 = std::cos(euler.y);
    c3 = std::cos(euler.z);

    s1 = std::sin(euler.x);
    s2 = std::sin(euler.y);
    s3 = std::sin(euler.z);

    // clang-format off
    return {
        {c2*c3, c3*s1*s2 - c1*s3,  s1*s3 + c1*c3*s2, 0.0f},
        {c2*s3, c1*c3 + s1*s2*s3,  c1*s2*s3 - c3*s1, 0.0f},
        { - s2,            c2*s1,             c1*c2, 0.0f},
        { 0.0f,             0.0f,              0.0f, 1.0f}
    };
    // clang-format on
}

template <typename T>
constexpr auto rotate(const Quaternion<T>& quatv) noexcept -> Matrix<T, 4> {
    auto    normalized_quatv = normalize(quatv);
    const T x = normalized_quatv.x, y = normalized_quatv.y, z = normalized_quatv.z, w = normalized_quatv.w;
    const T _2x2 = 2 * x * x, _2y2 = 2 * y * y, _2z2 = 2 * z * z, _2xy = 2 * x * y, _2xz = 2 * x * z,
            _2xw = 2 * x * w, _2yz = 2 * y * z, _2yw = 2 * y * w, _2zw = 2 * z * w;

    // clang-format off
    return {
        {1 - _2y2 - _2z2,     _2xy - _2zw,     _2xz + _2yw, 0},
        {    _2xy + _2zw, 1 - _2x2 - _2z2,     _2yz - _2xw, 0},
        {    _2xz - _2yw,     _2yz + _2xw, 1 - _2x2 - _2y2, 0},
        {0              , 0              , 0              , 1}
    };
    // clang-format on
}

template <typename T>
constexpr auto scale(T s) noexcept -> Matrix<T, 4> {
    // clang-format off
    return {
        { s, 0, 0, 0},
        { 0, s, 0, 0},
        { 0, 0, s, 0},
        { 0, 0, 0, 1},
    };
    // clang-format on
}

template <typename T>
constexpr auto scale(const Vector<T, 3>& v) noexcept -> Matrix<T, 4> {
    // clang-format off
    return {
        {v.x,   0,   0, 0},
        {  0, v.y,   0, 0},
        {  0,   0, v.z, 0},
        {  0,   0,   0, 1},
    };
    // clang-format on
}

template <typename T>
constexpr auto perspective_fov(T fov, T width, T height, T near, T far) noexcept -> Matrix<T, 4> {
    const T h   = std::tan(0.5 * fov);
    const T w   = h * width / height;
    const T nmf = near - far;

    // clang-format off
    return {
        {1 / w,     0,                  0,                    0},
        {    0, 1 / h,                  0,                    0},
        {    0,     0, (near + far) / nmf, 2 * near * far / nmf},
        {    0,     0,                 -1,                    0},
    };
    // clang-format on
}
template <typename T>
constexpr auto perspective(T fov, T aspect, T near, T far) noexcept -> Matrix<T, 4> {
    const T h   = std::tan(0.5 * fov);
    const T w   = h * aspect;
    const T nmf = near - far;

    // clang-format off
    return {
        {1 / w,     0,                  0,                    0},
        {    0, 1 / h,                  0,                    0},
        {    0,     0, (near + far) / nmf, 2 * near * far / nmf},
        {    0,     0,                 -1,                    0},
    };
    // clang-format on
}
template <typename T>
constexpr auto ortho(T left, T right, T bottom, T top, T near, T far) noexcept -> Matrix<T, 4> {
    // clang-format off
    return {
        {2 / (right - left),                  0,                0, (right + left) / (left - right)},
        {                 0, 2 / (top - bottom),                0, (top + bottom) / (bottom - top)},
        {                 0,                  0, 2 / (near - far),     (far + near) / (near - far)},
        {                 0,                  0,                0,                               1},
    };
    // clang-format on
}

template <typename T>
constexpr auto look_at(const Vector<T, 3>& position, const Vector<T, 3>& direction, const Vector<T, 3>& up) noexcept -> Matrix<T, 4> {
    Vector<T, 3> direct    = normalize(direction);
    Vector<T, 3> right     = normalize(cross(direct, up));
    Vector<T, 3> camera_up = normalize(cross(right, direct));
    // clang-format off
    return {
        {    right.x,     right.y,     right.z, -dot(right,     position)},
        {camera_up.x, camera_up.y, camera_up.z, -dot(camera_up, position)},
        {  -direct.x,   -direct.y,   -direct.z,  dot(direct,    position)},
        {          0,           0,           0,                         1}};
    // clang-format on
}

template <typename T>
constexpr auto get_translation(const Matrix<T, 4>& mat) noexcept -> Vector<T, 3> {
    return {mat[0][3], mat[1][3], mat[2][3]};
}

template <typename T>
constexpr auto get_scaling(const Matrix<T, 4>& mat) noexcept -> Vector<T, 3> {
    return {
        Vector<T, 3>(mat.col(0).xyz).norm(),
        Vector<T, 3>(mat.col(1).xyz).norm(),
        Vector<T, 3>(mat.col(2).xyz).norm(),
    };
}

template <typename T>
constexpr auto get_rotation(const Matrix<T, 4>& transform) noexcept -> Quaternion<T> {
    Vector<T, 3> scaling = get_scaling(transform);
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

    return euler_to_quaternion(rotation);
}

// Get right direction (a.k.a X-axis direction)
template <typename T>
constexpr auto get_right(const Matrix<T, 4>& transform) noexcept -> Vector<T, 3> {
    return normalize(Vector<T, 3>{
        transform[0][0],
        transform[1][0],
        transform[2][0],
    });
}

// Using for forward direction (a.k.a Z-axis direction)
template <typename T>
constexpr auto get_forward(const Matrix<T, 4>& transform) noexcept -> Vector<T, 3> {
    return normalize(Vector<T, 3>{
        transform[0][2],
        transform[1][2],
        transform[2][2],
    });
}

// Get up direction (a.k.a Y-axis direction)
template <typename T>
constexpr auto get_up(const Matrix<T, 4>& transform) noexcept -> Vector<T, 3> {
    return normalize(Vector<T, 3>{
        transform[0][1],
        transform[1][1],
        transform[2][1],
    });
}

// Return translation, rotation, scaling
template <typename T>
constexpr auto decompose(const Matrix<T, 4>& transform) noexcept -> std::tuple<Vector<T, 3>, Quaternion<T>, Vector<T, 3>> {
    return {get_translation(transform), get_rotation(transform), get_scaling(transform)};
}

template <typename T>
constexpr auto quaternion_to_axis_angle(const Quaternion<T>& quat) noexcept -> std::tuple<Vector<T, 3>, T> {
    T angle      = 2 * std::acos(quat.w);
    T inv_factor = static_cast<T>(1) / std::sqrt(static_cast<T>(1) - quat.w * quat.w);

    return {inv_factor * quat.xyz, angle};
}

template <typename T>
constexpr auto axis_angle_to_quternion(const Vector<T, 3>& axis, T angle) noexcept -> Quaternion<T> {
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
constexpr auto quaternion_to_euler(const Quaternion<T>& quat) noexcept -> Vector<T, 3> {
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
constexpr auto euler_to_quaternion(const Vector<T, 3>& euler) noexcept -> Quaternion<T> {
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
}  // namespace hitagi::math
