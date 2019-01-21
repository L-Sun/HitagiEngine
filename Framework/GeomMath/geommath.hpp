#pragma once
#include <iostream>
#include <cmath>
#include "include/Addition.h"
#include "include/Subtraction.h"
#include "include/Multiplication.h"
#include "include/Division.h"
#include "include/Vector.h"
#include "include/Matrix.h"
#include "include/DCT.h"
#include "include/Integral.h"
#include "include/Absolute.h"
#include "include/MaxMin.h"

namespace My {

const float PI = 3.14159265358979323846;

template <typename T, int ROWS, int COLS>
struct Matrix;

template <typename T, int D>
struct Vector {
    T data[D];
    Vector() = default;
    Vector(std::initializer_list<T> l) { std::move(l.begin(), l.end(), data); }
    Vector(const T& num) { std::fill(data, data + D, num); }
    Vector(const T* p) { std::copy(p, p + D, data); }
    Vector(const Vector& v) { std::copy(v.data, v.data + D, data); }
    Vector& operator=(const T* p) {
        std::copy(p, p + D, data);
        return *this;
    }
    Vector& operator=(const Vector& v) {
        std::copy(v.data, v.data + D, data);
        return *this;
    }

    operator T*() { return data; }
    operator const T*() const { return static_cast<const T*>(data); }

    friend std::ostream& operator<<(std::ostream& out, Vector v) {
        out << "(";
        for (size_t i = 0; i < D - 1; i++) {
            out << v.data[i] << ", ";
        }
        return out << v.data[D - 1] << ")";
    }

    const Vector operator+(const Vector& rhs) const {
        Vector result;
        ispc::AddByElement(data, rhs, result, D);
        return result;
    }
    const Vector operator+(const T& rhs) const {
        Vector result;
        ispc::AddByNum(data, rhs, result, D);
        return result;
    }
    friend const Vector operator+(const T& lhs, const Vector& rhs) {
        return rhs + lhs;
    }

    const Vector operator-() {
        Vector result;
        ispc::Negate(data, result, D);
        return result;
    }
    const Vector operator-(const Vector& rhs) const {
        Vector result;
        ispc::SubByElement(data, rhs, result, D);
        return result;
    }
    const Vector operator-(const T& rhs) const {
        Vector result;
        ispc::SubByNum(data, rhs, result, D);
        return result;
    }
    friend const Vector operator-(const T& lhs, const Vector& rhs) {
        Vector result;
        ispc::NegateSubByNum(lhs, rhs, result, D);
        return result;
    }

    const T operator*(const Vector& rhs) const {
        T result;
        ispc::DotProduct(data, rhs, &result, D);
        return result;
    }
    template <int DD>
    const Vector<T, DD> operator*(const Matrix<T, D, DD>& rhs) {
        Vector<T, DD> result;
        ispc::MatMul(data, rhs, result, 1, D, DD);
        return result;
    }
    const Vector operator*(const T& rhs) const {
        Vector result;
        ispc::MulByNum(data, rhs, result, D);
        return result;
    }
    friend const Vector operator*(const T& lhs, const Vector& rhs) {
        return rhs * lhs;
    }
    const Vector operator/(const T& rhs) const {
        Vector result;
        ispc::DivNum(data, rhs, result, D);
        return result;
    }

    Vector& operator+=(const Vector& rhs) {
        ispc::IncreaceByElement(data, rhs, D);
        return *this;
    }
    Vector& operator+=(const T& rhs) {
        ispc::IncreaceByNum(data, rhs, D);
        return *this;
    }
    Vector& operator-=(const Vector& rhs) {
        ispc::DecreaceByElement(data, rhs, D);
        return *this;
    }
    Vector& operator-=(const T& rhs) {
        ispc::DecreaceByNum(data, rhs, D);
        return *this;
    }
    Vector& operator*=(const T& rhs) {
        ispc::MulSelfByNum(data, rhs, D);
        return *this;
    }
    Vector& operator*=(const Matrix<T, D, D>& rhs) {
        return (*this = *this * rhs);
    }
    Vector& operator/=(const T& rhs) {
        ispc::DivSelfByNum(data, rhs, D);
        return *this;
    }
};

template <typename Vec, typename rT, typename T, int... Indexs>
class swizzle {
    friend Vec;

public:
    T*       data;
    swizzle& operator=(const T& v) {
        size_t indexs[] = {Indexs...};
        for (int i = 0; i < sizeof...(Indexs); i++) {
            data[indexs[i]] = v;
        }
        return *this;
    }
    swizzle& operator=(std::initializer_list<T> l) {
        size_t indexs[] = {Indexs...};
        size_t i        = 0;
        for (auto&& v : l) {
            data[indexs[i]] = v;
            i++;
        }
        return *this;
    }
    swizzle& operator=(const Vector<T, sizeof...(Indexs)>& v) {
        size_t indexs[] = {Indexs...};
        for (size_t i = 0; i < sizeof...(Indexs); i++) {
            data[indexs[i]] = v[i];
        }
        return *this;
    }

    operator rT() { return rT(data[Indexs]...); }
    operator const rT() const { return rT(data[Indexs]...); }

private:
    swizzle(Vec& v) : data(v.data) {}
};  // namespace My

template <typename T>
struct Vector2 : public Vector<T, 2> {
    typedef Vector<T, 2> base_vec;

    swizzle<Vector2, T, T, 0>             x  = *this;
    swizzle<Vector2, T, T, 1>             y  = *this;
    decltype(x)                           u  = x;
    decltype(y)                           v  = y;
    swizzle<Vector2, Vector2<T>, T, 0, 1> xy = *this;
    swizzle<Vector2, Vector2<T>, T, 1, 0> yx = *this;
    decltype(xy)                          uv = xy;

    Vector2() {}
    Vector2(const T* p) : base_vec(p) {}
    Vector2(const T& v) : base_vec(v) {}
    Vector2(const base_vec& v) : base_vec(v) {}
    Vector2(const T& x, const T& y) : base_vec({x, y}) {}
    Vector2(const Vector2& v) : base_vec(v) {}
    Vector2& operator=(const Vector2& v) {
        if (this != &v) base_vec::operator=(v);
        return *this;
    }
};
template <typename T>
struct Vector3 : public Vector<T, 3> {
    typedef Vector<T, 3> base_vec;

    swizzle<Vector3, T, T, 0> x = *this;
    swizzle<Vector3, T, T, 1> y = *this;
    swizzle<Vector3, T, T, 2> z = *this;
    decltype(x)               r = x;
    decltype(y)               g = y;
    decltype(z)               b = z;

    swizzle<Vector3, Vector2<T>, T, 0, 1>    xy  = *this;
    swizzle<Vector3, Vector2<T>, T, 1, 0>    yx  = *this;
    swizzle<Vector3, Vector2<T>, T, 2, 0>    zx  = *this;
    swizzle<Vector3, Vector2<T>, T, 0, 2>    xz  = *this;
    swizzle<Vector3, Vector2<T>, T, 1, 2>    yz  = *this;
    swizzle<Vector3, Vector2<T>, T, 2, 1>    zy  = *this;
    swizzle<Vector3, Vector3<T>, T, 0, 1, 2> xyz = *this;
    swizzle<Vector3, Vector3<T>, T, 0, 2, 1> xzy = *this;
    swizzle<Vector3, Vector3<T>, T, 1, 0, 2> yxz = *this;
    swizzle<Vector3, Vector3<T>, T, 1, 2, 0> yzx = *this;
    swizzle<Vector3, Vector3<T>, T, 2, 0, 1> zxy = *this;
    swizzle<Vector3, Vector3<T>, T, 2, 1, 0> zyx = *this;
    decltype(xyz)                            rgb = xyz;

    Vector3() {}
    Vector3(const T* p) : base_vec(p) {}
    Vector3(const T& v) : base_vec(v) {}
    Vector3(const base_vec& v) : base_vec(v) {}
    Vector3(const T& x, const T& y, const T& z) : base_vec({x, y, z}) {}
    Vector3(const Vector2<T>& v, const T& z) : base_vec({v.x, v.y, z}) {}
    Vector3(const Vector3& v) : base_vec(v) {}
    Vector3& operator=(const Vector3& v) {
        if (this != &v) base_vec::operator=(v);
        return *this;
    }
};
template <typename T>
struct Vector4 : public Vector<T, 4> {
    typedef Vector<T, 4> base_vec;

    swizzle<Vector4, T, T, 0> x = *this;
    swizzle<Vector4, T, T, 1> y = *this;
    swizzle<Vector4, T, T, 2> z = *this;
    swizzle<Vector4, T, T, 3> w = *this;
    decltype(x)               r = x;
    decltype(y)               g = y;
    decltype(z)               b = z;
    decltype(w)               a = w;

    swizzle<Vector4, Vector3<T>, T, 0, 1, 2> xyz = *this;
    swizzle<Vector4, Vector3<T>, T, 0, 2, 1> xzy = *this;
    swizzle<Vector4, Vector3<T>, T, 1, 0, 2> yxz = *this;
    swizzle<Vector4, Vector3<T>, T, 1, 2, 0> yzx = *this;
    swizzle<Vector4, Vector3<T>, T, 2, 0, 1> zxy = *this;
    swizzle<Vector4, Vector3<T>, T, 2, 1, 0> zyx = *this;
    decltype(xyz)                            rgb = xyz;

    swizzle<Vector4, Vector4<T>, T, 0, 1, 2, 3> rgba = *this;
    swizzle<Vector4, Vector4<T>, T, 2, 1, 0, 3> bgra = *this;

    Vector4() {}
    Vector4(const T* p) : base_vec(p) {}
    Vector4(const T& num) : base_vec(num) {}
    Vector4(const base_vec& v) : base_vec(v) {}
    Vector4(const T& x, const T& y, const T& z, const T& w)
        : base_vec({x, y, z, w}) {}
    Vector4(const Vector2<T>& v, const T& z, const T& w)
        : base_vec({v.x, v.y, z, w}) {}
    Vector4(const Vector3<T>& v, const T& w) : base_vec({v.x, v.y, v.z, w}) {}
    Vector4(const Vector4& v) : base_vec(v) {}
    Vector4& operator=(const Vector4& v) {
        if (this != &v) base_vec::operator=(v);
        return *this;
    }
};

typedef Vector2<float>   vec2;
typedef Vector3<float>   vec3;
typedef Vector4<float>   vec4;
typedef Vector4<float>   quat;
typedef Vector4<uint8_t> R8G8B8A8Unorm;

template <typename T, int ROWS, int COLS>
struct Matrix {
    typedef Vector<T, COLS> row_type;

    row_type data[ROWS];

    Matrix() = default;
    Matrix(const T v) { ispc::Identity(&data[0][0], v, ROWS); }
    Matrix(std::initializer_list<T> l) {
        if (l.size() != ROWS * COLS) {
            std::cerr << "can't match the size of matirx" << std::endl;
            std::cerr << "the all element of matrix has been init with 1.0 "
                      << std::endl;
            std::fill(data, data + ROWS, row_type((T)1.0));
        } else
            std::move(l.begin(), l.end(), &data[0][0]);
    }
    Matrix(std::initializer_list<row_type> l) {
        std::move(l.begin(), l.end(), data);
    }
    Matrix(const T (&a)[ROWS][COLS]) {
        std::copy(&a[0][0], &a[0][0] + ROWS * COLS, &data[0][0]);
    }
    Matrix(const T (&a)[ROWS * COLS]) {
        std::copy(a, a + ROWS * COLS, &data[0][0]);
    }
    Matrix(const T* p) { std::copy(p, p + ROWS * COLS, &data[0][0]); }

    row_type&       operator[](int row_index) { return data[row_index]; }
    const row_type& operator[](int row_index) const { return data[row_index]; }

    operator T*() { return &data[0][0]; }
    operator const T*() const { return static_cast<const T*>(&data[0][0]); }

    friend std::ostream& operator<<(std::ostream& out, const Matrix& mat) {
        std::fixed(out);
        for (int i = 0; i < ROWS; i++) {
            if (i == 0)
                out << "\n{{";
            else
                out << " {";
            for (int j = 0; j < COLS; j++) {
                out << mat[i][j];
                if (j != COLS - 1) out << ", ";
            }
            if (i != ROWS - 1)
                out << "},\n";
            else
                out << "}}\n";
        }
        return out;
    }

    // Matrix Operation
    const Matrix operator+(const Matrix& rhs) const {
        Matrix reuslt;
        ispc::AddByElement(*this, rhs, reuslt, ROWS * COLS);
        return reuslt;
    }
    const Matrix operator+(const T& rhs) const {
        Matrix result;
        ispc::AddByNum(*this, rhs, result, ROWS * COLS);
        return result;
    }
    friend const Matrix operator+(T lhs, const Matrix& rhs) {
        return rhs + lhs;
    }

    const Matrix operator-() const {
        Matrix result;
        ispc::Negate(*this, result, ROWS * COLS);
        return result;
    }
    const Matrix operator-(const Matrix& rhs) const {
        Matrix result;
        ispc::SubByElement(*this, rhs, result, ROWS * COLS);
        return result;
    }
    const Matrix operator-(const T& rhs) const {
        Matrix result;
        ispc::SubByNum(*this, rhs, result, ROWS * COLS);
        return result;
    }
    friend const Matrix operator-(T lhs, const Matrix& rhs) {
        Matrix result;
        ispc::NegateSubByNum(lhs, rhs, result, ROWS * COLS);
        return result;
    }
    template <int CC>
    const Matrix<T, ROWS, CC> operator*(const Matrix<T, COLS, CC>& rhs) const {
        Matrix<T, ROWS, CC> result;
        ispc::MatMul(*this, rhs, result, ROWS, COLS, CC);
        return result;
    }
    const row_type operator*(const row_type& rhs) const {
        row_type result;
        ispc::MatMul(*this, rhs, result, ROWS, COLS, 1);
        return result;
    }
    const Matrix operator*(const T& rhs) const {
        Matrix result;
        ispc::MulByNum(*this, rhs, result, ROWS * COLS);
        return result;
    }
    friend Matrix operator*(const T& lhs, const Matrix& rhs) {
        return rhs * lhs;
    }
    const Matrix operator/(const T& rhs) const {
        Matrix result;
        ispc::DivNum(*this, rhs, result, ROWS * COLS);
        return result;
    }

    Matrix& operator+=(const Matrix& rhs) {
        ispc::IncreaceByElement(*this, rhs, ROWS * COLS);
        return *this;
    }
    Matrix& operator+=(const T& rhs) {
        ispc::IncreaceByNum(*this, rhs, ROWS * COLS);
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) {
        ispc::DecreaceByElement(*this, rhs, ROWS * COLS);
        return *this;
    }
    Matrix& operator-=(const T& rhs) {
        ispc::DecreaceByNum(*this, rhs, ROWS * COLS);
        return *this;
    }
    Matrix& operator*=(const Matrix& rhs) { return (*this = *this * rhs); }
    Matrix& operator*=(const T& rhs) {
        ispc::MulSelfByNum(*this, rhs, ROWS * COLS);
        return *this;
    }
    Matrix& operator/=(const T& rhs) {
        ispc::DivSelfByNum(*this, rhs, ROWS * COLS);
        return *this;
    }

    const Matrix mulByElement(const Matrix& rhs) const {
        Matrix res;
        ispc::MulByElement(*this, rhs, res, ROWS * COLS);
        return res;
    }
};
typedef Matrix<float, 3, 3> mat3;
typedef Matrix<float, 4, 4> mat4;
typedef Matrix<float, 8, 8> mat8;

inline float radians(float angle) { return angle / 180.0f * PI; }

template <typename T, int D>
Vector<T, D> normalize(const Vector<T, D>& v) {
    Vector<T, D> res = v;
    ispc::Normalize(res, D);
    return res;
}
template <typename T>
Vector3<T> cross(const Vector3<T>& v1, const Vector3<T>& v2) {
    Vector3<T> result;
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
    ispc::MatrixExchangeYandZ(matrix, ROWS, COLS);
}

template <typename T>
Matrix<T, 4, 4> translate(const Matrix<T, 4, 4>& mat, const Vector3<T>& v) {
    // clang-format off
    Matrix<T, 4, 4> translation = {
        {1,   0,   0,   0},
        {0,   1,   0,   0},
        {0,   0,   1,   0},
        {v.x, v.y, v.z, 1}
    };
    // clang-format on
    return translation * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateX(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);
    // clang-format off
    Matrix<T, 4, 4> rotate_x = {
        {1,  0, 0, 0},
        {0,  c, s, 0},
        {0, -s, c, 0}, 
        {0,  0, 0, 1}
    };
    // clang-format on
    return rotate_x * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateY(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4, 4> rotate_y = {
        {c, 0, -s, 0},
        {0, 1,  0, 0},
        {s, 0,  c, 0},
        {0, 0,  0, 1}
    };
    // clang-format on
    return rotate_y * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateZ(const Matrix<T, 4, 4>& mat, const T angle) {
    const T c = std::cos(angle), s = std::sin(angle);

    // clang-format off
    Matrix<T, 4, 4> rotate_z = {
        { c, s, 0, 0},
        {-s, c, 0, 0},
        { 0, 0, 1, 0},
        { 0, 0, 0, 1}
    };
    // clang-format on

    return rotate_z * mat;
}
template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const T angle,
                       const Vector3<T>& axis) {
    const float c = cosf(angle), s = sinf(angle), _1_c = 1.0f - c;
    const float x = axis.x, y = axis.y, z = axis.z;
    // clang-format off
    Matrix<T, 4, 4> rotation = {
        {c + x * x * _1_c    , x * y * _1_c + z * s, x * z * _1_c - y * s, 0.0f},
        {x * y * _1_c - z * s, c + y * y * _1_c    , y * z * _1_c + x * s, 0.0f},
        {x * z * _1_c + y * s, y * z * _1_c - x * s, c + z * z * _1_c,     0.0f},
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
        {(cRoll * cYaw) + (sRoll * sPitch * sYaw) , (sRoll * cPitch), (cRoll * -sYaw) + (sRoll * sPitch * cYaw), 0.0f},
        {(-sRoll * cYaw) + (cRoll * sPitch * sYaw), (cRoll * cPitch), (sRoll * sYaw) + (cRoll * sPitch * cYaw) , 0.0f},
        {(cPitch * sYaw)                          , -sPitch         , (cPitch * cYaw)                          , 0.0f},
        {0.0f                                     , 0.0f            , 0.0f                                     , 1.0f}
    };
    // clang-format on

    return rotation * mat;
}

template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const Vector4<T>& quatv) {
    quatv     = normalize(quatv);
    const T a = quatv.x, b = quatv.y, c = quatv.z, d = quatv.w;
    const T _2a2 = 2 * a * a, _2b2 = 2 * b * b, _2c2 = 2 * c * c,
            _2ab = 2 * a * b, _2ac = 2 * a * c, _2ad = 2 * a * d,
            _2bc = 2 * b * c, _2bd = 2 * b * d, _2cd = 2 * c * d;

    // clang-format off
    Matrix<T, 4, 4> rotate_mat = {
        {1 - _2b2 - _2c2, _2ab - _2cd    , _2bd + _2ac    , 0},
        {_2ab + _2cd    , 1 - _2a2, -_2c2, _2bc - _2ad    , 0},
        {_2ac - _2bd    , _2ad + _2bc    , 1 - _2a2 - _2b2, 0},
        {0              , 0              , 0              , 1}
    };
    // clang-format on
    return rotate_mat * mat;
}
template <typename T>
Matrix<T, 4, 4> scale(const Matrix<T, 4, 4>& mat, const Vector3<T>& v) {
    auto res = mat;
    res[0] *= v.x;
    res[1] *= v.y;
    res[2] *= v.z;
    return res;
}

template <typename T>
Matrix<T, 4, 4> perspectiveFov(T fov, T width, T height, T near, T far) {
    Matrix<T, 4, 4> res(static_cast<T>(0));

    const T h   = std::tan(static_cast<T>(0.5) * fov);
    const T w   = h * height / width;
    const T nmf = near - far;

    res[0][0] = 1 / w;
    res[1][1] = 1 / h;
    res[2][2] = (near + far) / nmf;
    res[2][3] = static_cast<T>(-1);
    res[3][2] = static_cast<T>(2) * near * far / nmf;
    return res;
}
template <typename T>
Matrix<T, 4, 4> perspective(T fov, T aspect, T near, T far) {
    Matrix<T, 4, 4> res(static_cast<T>(0));

    const T h   = std::tan(static_cast<T>(0.5) * fov);
    const T w   = h * aspect;
    const T nmf = near - far;

    res[0][0] = static_cast<T>(1) / w;
    res[1][1] = static_cast<T>(1) / h;
    res[2][2] = (near + far) / nmf;
    res[2][3] = -static_cast<T>(1);
    res[3][2] = static_cast<T>(2) * near * far / nmf;
    return res;
}

template <typename T>
Matrix<T, 4, 4> lookAt(const Vector3<T>& position, const Vector3<T>& target,
                       const Vector3<T>& up) {
    Vector3<T> direct   = normalize(target - position);
    Vector3<T> right    = normalize(cross(direct, up));
    Vector3<T> cameraUp = normalize(cross(right, direct));

    Matrix<T, 4, 4> look_at = {
        {right.x, cameraUp.x, -direct.x, 0},
        {right.y, cameraUp.y, -direct.y, 0},
        {right.z, cameraUp.z, -direct.z, 0},
        {-right * position, -cameraUp * position, direct * position, 1}};
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

template <typename T, typename F>
const T integral(const T& a, const T& b, const T& precision, F&& func) {
    return ispc::Integral(a, b, precision, func);
}

template <typename T, int ROWS, int COLS>
const Vector3<T> GetOrigin(const Matrix<T, ROWS, COLS>& mat) {
    static_assert(
        ROWS >= 3,
        "[Error] Only 3x3 and above matrix can be passed to this method!");
    static_assert(
        COLS >= 3,
        "[Error] Only 3x3 and above matrix can be passed to this method!");
    return Vector3<T>(mat[3][0], mat[3][1], mat[3][2]);
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

    for (int i = 0; i < ROWS1; i++) {
        mat1[i] = mat2[i];
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
