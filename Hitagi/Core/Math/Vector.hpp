#pragma once
#include <iostream>
#include <array>
#include <cassert>

#include <fmt/format.h>
#include <fmt/ostream.h>

namespace Hitagi {
template <typename T, unsigned D>
struct Vector;

template <typename T, unsigned D, unsigned... Indexs>
class swizzle {
public:
    std::array<T, D> data;

    swizzle& operator=(const T& v) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        for (auto&& i : indexs) data[i] = v;
        return *this;
    }
    swizzle& operator=(const std::initializer_list<T>& l) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        unsigned                                          i      = 0;
        for (auto&& e : l) data[indexs[i++]] = e;
        return *this;
    }
    swizzle& operator=(const Vector<T, sizeof...(Indexs)>& v) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        for (auto&& i : indexs) data[i] = v[i];
        return *this;
    }

    operator Vector<T, sizeof...(Indexs)>() { return Vector<T, sizeof...(Indexs)>({data[Indexs]...}); }
    operator const Vector<T, sizeof...(Indexs)>() const { return Vector<T, sizeof...(Indexs)>({data[Indexs]...}); }
};

// clang-format off
template <typename T, unsigned D>
struct BaseVector {
    union {
         std::array<T, D> data;
    };
    BaseVector()=default;
};

template <typename T>
struct BaseVector<T, 2> {
    union {
        std::array<T, 2> data;
        struct { T x, y; };
        struct { T u, v; };
        swizzle<T, 2, 0, 1> xy, uv;
        swizzle<T, 2, 1, 0> yx, vu;
    };
    BaseVector()=default;
    BaseVector(const T& x, const T& y) : data{x, y} {}
};

template <typename T>
struct BaseVector<T, 3> {
    union {
        std::array<T, 3> data;
        struct { T x, y, z; };
        struct { T r, g, b; };
        swizzle<T, 3, 0, 1, 2> xyz, rgb;
        swizzle<T, 3, 0, 2, 1> xzy, rbg;
        swizzle<T, 3, 1, 0, 2> yxz, grb;
        swizzle<T, 3, 1, 2, 0> yzx, gbr;
        swizzle<T, 3, 2, 0, 1> zxy, brg;
        swizzle<T, 3, 2, 1, 0> zyx, bgr;
    };
    BaseVector()=default;
    BaseVector(const T& x, const T& y, const T& z) : data{x, y, z} {}
};

template <typename T>
struct BaseVector<T, 4> {
    union {
        std::array<T, 4> data;
        struct { T x, y, z, w; };
        struct { T r, g, b, a; };
        swizzle<T, 3, 0, 1, 2> xyz, rgb;
        swizzle<T, 3, 0, 2, 1> xzy, rbg;
        swizzle<T, 3, 1, 0, 2> yxz, grb;
        swizzle<T, 3, 1, 2, 0> yzx, gbr;
        swizzle<T, 3, 2, 0, 1> zxy, brg;
        swizzle<T, 3, 2, 1, 0> zyx, bgr;
        swizzle<T, 4, 0, 1, 2, 3> xyzw, rgba;
        swizzle<T, 4, 2, 1, 0, 3> zyxw, bgra;
    };
    BaseVector()=default;
    BaseVector(const T& x, const T& y, const T& z,const T& w) : data{x, y, z, w} {}
};
// clang-format on

template <typename T, unsigned D>
struct Vector : public BaseVector<T, D> {
    using BaseVector<T, D>::data;
    using BaseVector<T, D>::BaseVector;

    Vector(const Vector&) = default;
    Vector(Vector&&)      = default;
    Vector& operator=(const Vector&) = default;
    Vector& operator=(Vector&&) = default;

    explicit Vector(const T& num) { data.fill(num); }
    Vector(const T (&a)[D]) { std::copy_n(a, D, data.begin()); }

    Vector(const Vector<T, D - 1>& v, const T& num) {
        std::copy_n(v.data.begin(), D - 1, data.begin());
        data[D - 1] = num;
    }

    template <typename TT>
    Vector(const TT* p) {
        for (unsigned i = 0; i < D; i++) data[i] = static_cast<T>(*p++);
    }

    T norm() const {
        T result = 0;
        for (auto&& val : data) result += val * val;
        return std::sqrt(result);
    }
    Vector& operator=(const T (&a)[D]) {
        std::copy_n(a, D, data.begin());
        return *this;
    }

    operator T*() { return data.data(); }
    operator const T*() const { return data.data(); }

    T&       operator[](unsigned index) { return data[index]; }
    const T& operator[](unsigned index) const { return data[index]; }

    friend std::ostream& operator<<(std::ostream& out, Vector v) {
        return out << fmt::format("[{:6.2f}]", fmt::join(v.data, ", ")) << std::flush;
    }

    const Vector operator+(const Vector& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] + rhs[i];
        return result;
    }
    const Vector operator+(const T& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] + rhs;
        return result;
    }
    friend const Vector operator+(const T& lhs, const Vector& rhs) { return rhs + lhs; }

    const Vector operator-() const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = -data[i];
        return result;
    }
    const Vector operator-(const Vector& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] - rhs[i];
        return result;
    }
    const Vector operator-(const T& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] - rhs;
        return result;
    }
    friend const Vector operator-(const T& lhs, const Vector& rhs) {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = lhs - rhs[i];
        return result;
    }

    const Vector operator*(const Vector& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] * rhs[i];
        return result;
    }
    const Vector operator*(const T& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] * rhs;
        return result;
    }
    friend const Vector operator*(const T& lhs, const Vector& rhs) { return rhs * lhs; }
    const Vector        operator/(const T& rhs) const {
        Vector result;
        for (unsigned i = 0; i < D; i++) result[i] = data[i] / rhs;
        return result;
    }

    Vector& operator+=(const Vector& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] += rhs[i];
        return *this;
    }
    Vector& operator+=(const T& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] += rhs;
        return *this;
    }
    Vector& operator-=(const Vector& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] -= rhs[i];
        return *this;
    }
    Vector& operator-=(const T& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] -= rhs;
        return *this;
    }
    Vector& operator*=(const T& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] *= rhs;
        return *this;
    }
    Vector& operator/=(const T& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] /= rhs;
        return *this;
    }
};

template <typename T, unsigned D>
T dot(const Vector<T, D>& v1, const Vector<T, D>& v2) {
    T result = 0;
    for (unsigned i = 0; i < D; i++) result += v1[i] * v2[i];
    return result;
}
}  // namespace Hitagi
