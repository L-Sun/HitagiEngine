#pragma once
#include <cstring>
#include <iostream>
#include "include/Addition.h"
#include "include/Subtraction.h"
#include "include/Multiplication.h"

namespace My {

template <typename T, size_t SizeOfArray>
constexpr size_t countof(T (&array)[SizeOfArray]) {
    return SizeOfArray;
}
template <typename T, size_t RowSize, size_t ColSize>
constexpr size_t countof(T (&)[RowSize][ColSize]) {
    return RowSize * ColSize;
}

template <template <typename> class TT, typename T>
std::ostream& operator<<(std::ostream& out, TT<T> vector) {
    out << "( ";
    for (uint32_t i = 0; i < countof(vector.data); i++) {
        out << vector.data[i] << ((i == countof(vector.data) - 1) ? ' ' : ',');
    }
    out << ")";
    return out;
}

template <template <typename> class TT, typename T, int... Indexs>
class swizzle {
private:
    T v[sizeof...(Indexs)];

public:
    TT<T>& operator=(const TT<T>& rhs) {
        int indexs[] = {Indexs...};

        for (int i = 0; i < sizeof...(Indexs); i++) {
            v[indexs[i]] = rhs[i];
        }
        return *(TT<T>*)this;
    }
    operator TT<T>() const { return TT<T>(v[Indexs]...); }
};

template <typename T>
struct Vector2 {
    union {
        // clang-format off
        T data[2];
        struct { T x, y; };
        struct { T r, g; };
        struct { T u, v; };
        swizzle<Vector2, T, 0, 1> xy;
        swizzle<Vector2, T, 1, 0> yx;
        // clang-format on
    };

    Vector2() {}
    Vector2(const T& _v) : x(_v), y(_v) {}
    Vector2(const T& _x, const T& _y) : x(_x), y(_y) {}

    operator T*() { return data; }
    operator const T*() const { return static_cast<const T*>(data); }
};

typedef Vector2<float> vec2;

template <typename T>
struct Vector3 {
    union {
        // clang-format off
        T data[3];
        struct { T x, y, z; };
        struct { T r, g, b; };
        swizzle<Vector3, T, 0, 1> xy;
        swizzle<Vector3, T, 1, 0> yx;
        swizzle<Vector3, T, 0, 2> xz;
        swizzle<Vector3, T, 2, 0> zx;
        swizzle<Vector3, T, 1, 2> yz;
        swizzle<Vector3, T, 2, 1> zy;
        swizzle<Vector3, T, 0, 1, 2> xyz;
        swizzle<Vector3, T, 1, 2, 0> yzx;
        swizzle<Vector3, T, 2, 0, 1> zxy;
        swizzle<Vector3, T, 0, 2, 1> xzy;
        swizzle<Vector3, T, 1, 0, 2> yxz;
        swizzle<Vector3, T, 2, 1, 0> zyx;

        // clang-format on
    };

    Vector3() {}
    Vector3(const T& _v) : x(_v), y(_v), z(_v) {}
    Vector3(const T& _x, const T& _y, const T& _z) : x(_x), y(_y), z(_z) {}

    operator T*() { return data; }
    operator const T*() const { return static_cast<const T*>(data); }
};
typedef Vector3<float> vec3;

template <typename T>
struct Vector4 {
    union {
        // clang-format off
        T data[2];
        struct { T x, y, z, w; };
        struct { T r, g, b, a; };
        swizzle<Vector3, T, 0, 1, 2> xyz;
        swizzle<Vector3, T, 1, 2, 0> yzx;
        swizzle<Vector3, T, 2, 0, 1> zxy;
        swizzle<Vector3, T, 0, 2, 1> xzy;
        swizzle<Vector3, T, 1, 0, 2> yxz;
        swizzle<Vector3, T, 2, 1, 0> zyx;
        swizzle<Vector3, T, 2, 1, 0, 3> bgra;
        // clang-format on
    };

    Vector4() {}
    Vector4(const T& _v) : x(_v), y(_v), z(_v), w(_v) {}
    Vector4(const T& _x, const T& _y, const T& _z, const T& _w)
        : x(_x), y(_y), z(_z), w(_w) {}
    Vector4(const Vector3<T>& v3, const T& _w)
        : x(v3.x), y(v3.y), z(v3.z), w(_w) {}
    Vector4(const Vector3<T>& v3) : x(v3.x), y(v3.y), z(v3.z), w(1.0f) {}

    operator T*() { return data; }
    operator const T*() const { return static_cast<const T*>(data); }

    Vector4& operator=(const T* f) {
        std::memcpy(data, f, sizeof(T) * 4);
        return *this;
    }
};
typedef Vector4<float> vec4;

// Vector Operation
template <template <typename> class Vec, typename T>
Vec<T> operator+(const Vec<T>& lhs, const Vec<T>& rhs) {
    Vec<T> result;
    ispc::AddByElement(lhs, rhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T>& operator+=(Vec<T>& lhs, const Vec<T>& rhs) {
    ispc::IncreaceByElement(lhs, rhs, countof(lhs.data));
    return lhs;
}
template <template <typename> class Vec, typename T>
Vec<T> operator-(const Vec<T>& rhs) {
    Vec<T> result;
    ispc::Negate(rhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T> operator-(const Vec<T>& lhs, const Vec<T>& rhs) {
    Vec<T> result;
    ispc::SubByElement(lhs, rhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T>& operator-=(Vec<T>& lhs, const Vec<T>& rhs) {
    ispc::DecreaceByElement(lhs, rhs, countof(lhs.data));
    return lhs;
}
template <template <typename> class Vec, typename T>
Vec<T> operator*(const Vec<T>& lhs, const Vec<T>& rhs) {
    Vec<T> result;
    ispc::DotProduct(lhs, rhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T> operator*(const Vec<T>& lhs, const T& rhs) {
    Vec<T> result;
    ispc::MulByNum(lhs, rhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T> operator*(const T& lhs, const Vec<T>& rhs) {
    Vec<T> result;
    ispc::MulByNum(rhs, lhs, result, countof(result.data));
    return result;
}
template <template <typename> class Vec, typename T>
Vec<T>& operator*=(Vec<T>& lhs, const T& rhs) {
    ispc::MulSelfByNum(lhs, rhs, countof(lhs.data));
    return lhs;
}

template <template <typename> class Vec, typename T>
Vec<T> cross(const Vec<T>& vec1, const Vec<T>& vec2) {
    Vec<T> result;
    ispc::CrossProduct(vec1, vec2, result, countof(result));
    return result;
}

template <typename T, int ROWS, int COLS>
struct Matrix {
    union {
        T data[ROWS][COLS];
    };
    auto       operator[](int row_index) { return data[row_index]; }
    const auto operator[](int row_index) const { return data[row_index]; }

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
    const Matrix operator+(const T& rhs) const {
        Matrix result;
        ispc::AddByNum(*this, rhs, result, countof(result.data));
        return result;
    }
    friend const Matrix operator+(const T& lhs, const Matrix& rhs) {
        return rhs + lhs;
    }
    Matrix& operator+=(const Matrix& rhs) {
        ispc::IncreaceByElement(*this, rhs, countof(data));
        return *this;
    }
    Matrix& operator+=(const T& rhs) {
        ispc::IncreaceByNum(*this, rhs, countof(data));
        return *this;
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
    const Matrix operator-(const T& rhs) const {
        Matrix result;
        ispc::SubByNum(*this, rhs, result, countof(result.data));
        return result;
    }
    friend const Matrix operator-(const T& lhs, const Matrix& rhs) {
        return rhs - lhs;
    }
    Matrix& operator-=(const T& rhs) {
        ispc::DecreaceByNum(*this, rhs, countof(data));
        return *this;
    }
};
typedef Matrix<float, 3, 3> mat3;
typedef Matrix<float, 4, 4> mat4;

}  // namespace My
