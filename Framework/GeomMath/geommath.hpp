#pragma once
#include <iostream>
#include <cmath>
#include "include/Addition.h"
#include "include/Subtraction.h"
#include "include/Multiplication.h"
#include "include/Division.h"
#include "include/Vector.h"

namespace My {

static constexpr const float PI = 3.14159265358979323846;

template <typename T, int ROWS, int COLS>
struct Matrix;

template <typename T, int D>
struct Vector {
    T data[D];
    Vector() = default;
    Vector(std::initializer_list<T> l) { std::move(l.begin(), l.end(), data); }
    Vector(const T& num) { std::fill(data, data + D, num); }
    Vector(const T* p) { std::copy(p, p + D, data); }
    template <int DD>
    Vector(const Vector<T, DD>& v) {
        if (DD < D) {
            std::copy(v.data, v.data + DD, data);
            std::fill(data + DD, data + D, 0.0);
        } else
            std::copy(v.data, v.data + D, data);
    }
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
    T* data;

    swizzle& operator=(std::initializer_list<T> l) {
        size_t indexs[] = {Indexs...};
        size_t i        = 0;
        for (auto&& v : l) {
            data[indexs[i]] = v;
            i++;
        }
        return *this;
    }

    operator rT() { return rT(data[Indexs]...); }
    operator const rT() const { return rT(data[Indexs]...); }

private:
    swizzle(Vec& v) : data(v.data) {}
};

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

    Vector2() = default;
    Vector2(const T* p) : base_vec(p) {}
    Vector2(const T& v) : base_vec(v) {}
    Vector2(const T& x, const T& y) : base_vec({x, y}) {}
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

    Vector3() = default;
    Vector3(const T* p) : base_vec(p) {}
    Vector3(const T& v) : base_vec(v) {}
    template <int DD>
    Vector3(const Vector<T, DD>& v) : base_vec(v) {}
    Vector3(const T& x, const T& y, const T& z) : base_vec({x, y, z}) {}
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

    Vector4() = default;
    Vector4(const T* p) : base_vec(p) {}
    Vector4(const T& num) : base_vec(num) {}
    template <int DD>
    Vector4(const Vector<T, DD>& v) : base_vec(v) {}
    Vector4(const T& x, const T& y, const T& z, const T& w)
        : base_vec({x, y, z, w}) {}
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
    Matrix(const T v) {
        for (size_t i = 0; i < ROWS; i++)
            for (size_t j = 0; j < COLS; j++) data[i][j] = i == j ? v : 0.0;
    }
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
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                out << mat[i][j] << " ";
            }
            out << '\n';
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
    const Matrix operator/(const T& rhs) {
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
};
typedef Matrix<float, 3, 3> mat3;
typedef Matrix<float, 4, 4> mat4;

float radians(float angle) { return angle * PI / 180.0f; }

template <typename T, int D>
Vector<T, D> normalize(const Vector<T, D>& v) {
    Vector<T, D> ret = v;
    ispc::Normalize(ret, D);
    return ret;
}
template <typename T>
Vector3<T> cross(const Vector3<T>& v1, const Vector3<T>& v2) {
    Vector3<T> result;
    ispc::CrossProduct(v1, v2, result);
    return result;
}

template <typename T>
Matrix<T, 4, 4> translate(const Matrix<T, 4, 4>& mat, const Vector3<T>& v) {
    Matrix<T, 4, 4> ret(mat);
    ret[0] = v.x * mat[3];
    ret[1] = v.y * mat[3];
    ret[2] = v.z * mat[3];
    return ret;
}
template <typename T>
Matrix<T, 4, 4> rotateX(const Matrix<T, 4, 4>& mat, const float angle) {
    float c = cosf(angle), s = sinf(angle);

    Matrix<T, 4, 4> ret = {
        {1, 0, 0, 0}, {0, c, -s, 0}, {0, s, c, 0}, {0, 0, 0, 1}};
    return ret * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateY(const Matrix<T, 4, 4>& mat, const float angle) {
    float c = cosf(angle), s = sinf(angle);

    Matrix<T, 4, 4> ret = {
        {c, 0, s, 0}, {0, 1, 0, 0}, {-s, 0, c, 0}, {0, 0, 0, 1}};
    return ret * mat;
}
template <typename T>
Matrix<T, 4, 4> rotateZ(const Matrix<T, 4, 4>& mat, const float angle) {
    float c = cosf(angle), s = sinf(angle);

    Matrix<T, 4, 4> ret = {
        {c, -s, 0, 0}, {s, c, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    return ret * mat;
}
template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const float angle,
                       const Vector3<T>& axis) {
    float        c = cosf(angle), s = sinf(angle);
    float        _1c = 1 - c;
    const float &x = axis.x, &y = axis.y, &z = axis.z;
    float        xz = x * z, xy = x * y, yz = y * z;

    Matrix<T, 4, 4> ret = {
        {c + x * x * _1c, xy * _1c - z * s, xz * _1c + y * s, 0},
        {xy * _1c + z * s, c + y * y * _1c, yz * _1c - x * s, 0},
        {xz * _1c - y * s, yz * _1c + x * s, c * z * z * _1c, 0},
        {0, 0, 0, 1}};
    return ret * mat;
}

template <typename T>
Matrix<T, 4, 4> rotate(const Matrix<T, 4, 4>& mat, const Vector4<T>& quatv) {
    return Matrix<T, 4, 4>(1.0f);
}
template <typename T>
Matrix<T, 4, 4> scale(const Matrix<T, 4, 4>& mat, const Vector3<T>& v) {
    auto ret = mat;
    ret[0] *= v.x;
    ret[1] *= v.y;
    ret[2] *= v.z;
    return ret;
}

mat4 respective(unsigned int width, unsigned int height, float near,
                float far) {
    mat4  ret(0.0f);
    float nmf = near - far;
    ret[0][0] = 2.0f * near / width;
    ret[1][1] = 2.0f * near / height;
    ret[2][2] = (near + far) / nmf;
    ret[2][3] = 2.0f * near * far / nmf;
    ret[3][2] = -1.0f;
    return ret;
}
mat4 respective(float fov, float aspect, float near, float far) {
    unsigned int height = 2 * near / tanf(fov / 2);
    unsigned int width  = height * aspect;
    return respective(width, height, near, far);
}
mat4 orth(float left, float right, float top, float bottom, float near,
          float far) {
    mat4 ret(0.0f);
    ret[0][0] = 2 / (right - left);
    ret[1][1] = 2 / (top - bottom);
    ret[2][2] = -2 / (far - near);
    ret[2][3] = -(far + near) / (far - near);
    ret[3][3] = 1;
    return ret;
}

mat4 lookAt(const vec3& position, const vec3& target, const vec3& up) {
    vec3 direct   = normalize(target - position);
    vec3 right    = normalize(cross(direct, up));
    vec3 cameraUp = normalize(cross(right, direct));
    mat4 l(1.0f);
    mat4 r(1.0f);
    l[0]    = vec4(right);
    l[1]    = vec4(cameraUp);
    l[2]    = vec4(direct);
    r[0][3] = -position.x;
    r[1][3] = -position.y;
    r[2][3] = -position.z;
    return l * r;
}

}  // namespace My
