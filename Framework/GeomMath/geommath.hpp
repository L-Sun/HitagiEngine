#pragma once
#include <iostream>
#include <cassert>
#include "include/Addition.h"
#include "include/Subtraction.h"
#include "include/Multiplication.h"
#include "include/Division.h"

namespace My {
template <typename T, size_t RowSize, size_t ColSize>
constexpr size_t countof(T (&)[RowSize][ColSize]) {
    return RowSize * ColSize;
}

template <typename T, int D>
struct Vector {
    T data[D];
    Vector() = default;
    Vector(std::initializer_list<T> l) { std::move(l.begin(), l.end(), data); }
    Vector(const T& num) { std::fill(data, data + D, num); }

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
    Vector& operator/=(const T& rhs) {
        ispc::DivSelfByNum(data, rhs, D);
        return *this;
    }
};

template <typename Vec, typename rVec, typename T, int... Indexs>
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

    operator rVec() { return rVec(data[Indexs]...); }
    operator const rVec() const { return rVec(data[Indexs]...); }

private:
    swizzle(Vec& v) : data(v.data) {}
};

template <typename T>
struct Vector2 : public Vector<T, 2> {
    typedef Vector<T, 2> base_vec;

    T& x = base_vec::data[0];
    T& y = base_vec::data[1];
    T &u = x, v = y;

    swizzle<Vector2, Vector2<T>, T, 0, 1> xy = *this;
    swizzle<Vector2, Vector2<T>, T, 1, 0> yx = *this;
    decltype(xy)                          uv = xy;

    Vector2() = default;
    Vector2(const T& v) : base_vec(v) {}
    Vector2(const T& x, const T& y) : base_vec({x, y}) {}
};
template <typename T>
struct Vector3 : public Vector<T, 3> {
    typedef Vector<T, 3> base_vec;

    T& x = base_vec::data[0];
    T& y = base_vec::data[1];
    T& z = base_vec::data[2];
    T &r = x, g = y, b = z;

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
    decltype(xyz)&                           rgb = xyz;

    Vector3() = default;
    Vector3(const T& v) : base_vec(v) {}
    Vector3(const T& x, const T& y, const T& z) : base_vec({x, y, z}) {}
};
template <typename T>
struct Vector4 : public Vector<T, 4> {
    typedef Vector<T, 4> base_vec;

    T& x = base_vec::data[0];
    T& y = base_vec::data[1];
    T& z = base_vec::data[2];
    T& w = base_vec::data[3];
    T &r = x, g = y, b = z, a = w;

    swizzle<Vector4, Vector3<T>, T, 0, 1, 2> xyz = *this;
    swizzle<Vector4, Vector3<T>, T, 0, 2, 1> xzy = *this;
    swizzle<Vector4, Vector3<T>, T, 1, 0, 2> yxz = *this;
    swizzle<Vector4, Vector3<T>, T, 1, 2, 0> yzx = *this;
    swizzle<Vector4, Vector3<T>, T, 2, 0, 1> zxy = *this;
    swizzle<Vector4, Vector3<T>, T, 2, 1, 0> zyx = *this;
    decltype(xyz)&                           rgb = xyz;

    swizzle<Vector4, Vector4<T>, T, 0, 1, 2, 3> rgba = *this;
    swizzle<Vector4, Vector4<T>, T, 2, 1, 0, 3> bgra = *this;

    Vector4() = default;
    Vector4(const T& x, const T& y, const T& z, const T& w)
        : base_vec({x, y, z, w}) {}
};

typedef Vector2<float> vec2;
typedef Vector3<float> vec3;
typedef Vector4<float> vec4;

template <typename T, int ROWS, int COLS>
struct Matrix {
    typedef Vector<T, COLS> row_type;

    row_type data[ROWS];

    Matrix() : Matrix((T)1.0) {}
    Matrix(const T& v) { std::fill(data, data + ROWS, row_type(v)); }
    Matrix(std::initializer_list<row_type> l) {
        std::move(l.begin(), l.end(), data);
    }
    Matrix& operator=(const T* _data) {
        std::copy(_data, _data + COLS * ROWS, data);
        return *this;
    }

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
        ispc::AddByElement(*this, rhs, reuslt, countof(reuslt.data));
        return reuslt;
    }
    const Matrix operator+(float rhs) const {
        Matrix result;
        ispc::AddByNum(*this, rhs, result, countof(result.data));
        return result;
    }
    friend const Matrix operator+(float lhs, const Matrix& rhs) {
        return rhs + lhs;
    }

    const Matrix operator-() const {
        Matrix result;
        ispc::Negate(*this, result, countof(result.data));
        return result;
    }
    const Matrix operator-(const Matrix& rhs) const {
        Matrix result;
        ispc::SubByElement(*this, rhs, result, countof(result.data));
        return result;
    }
    const Matrix operator-(float rhs) const {
        Matrix result;
        ispc::SubByNum(*this, rhs, result, countof(result.data));
        return result;
    }
    friend const Matrix operator-(float lhs, const Matrix& rhs) {
        return rhs - lhs;
    }

    Matrix& operator+=(const Matrix& rhs) {
        ispc::IncreaceByElement(*this, rhs, countof(data));
        return *this;
    }
    Matrix& operator+=(float rhs) {
        ispc::IncreaceByNum(*this, rhs, countof(data));
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) {
        ispc::DecreaceByElement(*this, rhs, countof(data));
        return *this;
    }
    Matrix& operator-=(float rhs) {
        ispc::DecreaceByNum(*this, rhs, countof(data));
        return *this;
    }
};
typedef Matrix<float, 3, 3> mat3;
typedef Matrix<float, 4, 4> mat4;

}  // namespace My
