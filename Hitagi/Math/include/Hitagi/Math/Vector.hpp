#pragma once
#include <iostream>
#include <array>
#include <cassert>
#include <concepts>

#include <fmt/format.h>

#if defined(USE_ISPC)
#include "ispcMath.hpp"
#endif  // USE_ISPC

namespace hitagi::math {
template <typename T, unsigned D>
struct Vector;

template <typename T, unsigned D, unsigned... Indexs>
class Swizzle {
public:
    std::array<T, D> data;

    Swizzle& operator=(const T& v) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        for (auto&& i : indexs) data[i] = v;
        return *this;
    }
    Swizzle& operator=(const std::initializer_list<T>& l) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        unsigned                                          i      = 0;
        for (auto&& e : l) data[indexs[i++]] = e;
        return *this;
    }
    Swizzle& operator=(const Vector<T, sizeof...(Indexs)>& v) {
        constexpr std::array<unsigned, sizeof...(Indexs)> indexs = {Indexs...};
        for (auto&& i : indexs) data[i] = v[i];
        return *this;
    }

    operator Vector<T, sizeof...(Indexs)>() const noexcept { return Vector<T, sizeof...(Indexs)>{data[Indexs]...}; }
};

// clang-format off
template <typename T, unsigned D>
struct BaseVector {
    union {
         std::array<T, D> data;
    };
    BaseVector()=default;
    BaseVector(std::array<T,D> a):data{a}{}
};

template <typename T>
struct BaseVector<T, 2> {
    union {
        std::array<T, 2> data;
        struct { T x, y; };
        struct { T u, v; };
        Swizzle<T, 2, 0, 1> xy, uv;
        Swizzle<T, 2, 1, 0> yx, vu;
    };
    BaseVector()=default;
    BaseVector(std::array<T,2> a):data{a}{}
    BaseVector(const T& x, const T& y) : data{x, y} {}
};

template <typename T>
struct BaseVector<T, 3> {
    union {
        std::array<T, 3> data;
        struct { T x, y, z; };
        struct { T r, g, b; };
        Swizzle<T, 3, 0, 1> xy, uv;
        Swizzle<T, 3, 0, 1, 2> xyz, rgb;
        Swizzle<T, 3, 0, 2, 1> xzy, rbg;
        Swizzle<T, 3, 1, 0, 2> yxz, grb;
        Swizzle<T, 3, 1, 2, 0> yzx, gbr;
        Swizzle<T, 3, 2, 0, 1> zxy, brg;
        Swizzle<T, 3, 2, 1, 0> zyx, bgr;
    };
    BaseVector()=default;
    BaseVector(std::array<T,3> a):data{a}{}
    BaseVector(const T& x, const T& y, const T& z) : data{x, y, z} {}
};

template <typename T>
struct BaseVector<T, 4> {
    union {
        std::array<T, 4> data;
        struct { T x, y, z, w; };
        struct { T r, g, b, a; };
        Swizzle<T, 4, 0, 1> xy, uv;
        Swizzle<T, 4, 0, 1, 2> xyz, rgb;
        Swizzle<T, 4, 0, 2, 1> xzy, rbg;
        Swizzle<T, 4, 1, 0, 2> yxz, grb;
        Swizzle<T, 4, 1, 2, 0> yzx, gbr;
        Swizzle<T, 4, 2, 0, 1> zxy, brg;
        Swizzle<T, 4, 2, 1, 0> zyx, bgr;
        Swizzle<T, 4, 0, 1, 2, 3> xyzw, rgba;
        Swizzle<T, 4, 2, 1, 0, 3> zyxw, bgra;
    };
    BaseVector()=default;
    BaseVector(std::array<T,4> a):data{a}{}
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

    Vector(const Vector<T, D - 1>& v, const T& num) {
        std::copy_n(v.data.begin(), D - 1, data.begin());
        data[D - 1] = num;
    }

    template <typename TT>
    Vector(const TT* p) {
        for (unsigned i = 0; i < D; i++) data[i] = static_cast<T>(*p++);
    }

    const T norm() const noexcept {
        T result = 0;
        for (auto&& val : data) result += val * val;
        return std::sqrt(result);
    }

    operator T*() { return data.data(); }
    operator const T*() const { return data.data(); }

    T&       operator[](unsigned index) noexcept { return data[index]; }
    const T& operator[](unsigned index) const noexcept { return data[index]; }

    friend std::ostream& operator<<(std::ostream& out, const Vector& v) {
        return out << fmt::format("[{:6}]", fmt::join(v.data, ", ")) << std::flush;
    }

    Vector operator+(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] + rhs[i];
        return result;
    }

    Vector operator-() const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = -data[i];
        return result;
    }
    Vector operator-(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] - rhs[i];
        return result;
    }

    Vector operator*(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] * rhs[i];
        return result;
    }
    Vector operator*(const T& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] * rhs;
        return result;
    }
    friend Vector operator*(const T& lhs, const Vector& rhs) noexcept { return rhs * lhs; }
    Vector        operator/(const T& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] / rhs;
        return result;
    }
    Vector operator/(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] / rhs[i];
        return result;
    }
    Vector& operator+=(const Vector& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] += rhs[i];
        return *this;
    }
    Vector& operator-=(const Vector& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] -= rhs[i];
        return *this;
    }
    Vector& operator*=(const T& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] *= rhs;
        return *this;
    }
    Vector& operator/=(const T& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] /= rhs;
        return *this;
    }

#if defined(USE_ISPC)
    Vector operator+(const Vector& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_add(*this, rhs, result, D);
        return result;
    }

    Vector operator-() const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_inverse(*this, result, D);
        return result;
    }
    Vector operator-(const Vector& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_sub(*this, rhs, result, D);
        return result;
    }
    Vector operator*(const Vector& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_mult_vector(*this, rhs, result, D);
        return result;
    }
    Vector operator*(const T& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_mult(*this, rhs, result, D);
        return result;
    }
    Vector operator/(const T& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_div(*this, rhs, result, D);
        return result;
    }
    Vector operator/(const Vector& rhs) const noexcept requires IspcSpeedable<T> {
        Vector result;
        ispc::vector_div_vector(*this, rhs, result, D);
        return result;
    }
    Vector& operator+=(const Vector& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_add_assgin(*this, rhs, D);
        return *this;
    }
    Vector& operator-=(const Vector& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_sub_assgin(*this, rhs, D);
        return *this;
    }
    Vector& operator*=(const T& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_mult_assgin(*this, rhs, D);
        return *this;
    }
    Vector& operator/=(const T& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_div_assign(*this, rhs, D);
        return *this;
    }
#endif  // USE_ISPC
};

template <typename T>
struct Quaternion : public Vector<T, 4> {
    using Vector<T, 4>::Vector;
    using Vector<T, 4>::data;

    Quaternion operator*(const Quaternion& rhs) const noexcept {
        return {
            data[0] * rhs.data[3] + data[3] * rhs.data[0] + data[1] * rhs.data[2] - data[2] * rhs.data[1],
            data[1] * rhs.data[3] + data[3] * rhs.data[1] + data[2] * rhs.data[0] - data[0] * rhs.data[2],
            data[2] * rhs.data[3] + data[3] * rhs.data[2] + data[0] * rhs.data[1] - data[1] * rhs.data[0],
            data[3] * rhs.data[3] - data[0] * rhs.data[0] - data[1] * rhs.data[1] - data[2] * rhs.data[2],
        };
    }
};

using vec2f = Vector<float, 2>;
using vec3f = Vector<float, 3>;
using vec4f = Vector<float, 4>;

using vec2d = Vector<double, 2>;
using vec3d = Vector<double, 3>;
using vec4d = Vector<double, 4>;

using quatf = Quaternion<float>;
using quatd = Quaternion<double>;

using R8G8B8A8Unorm = Vector<uint8_t, 4>;

template <typename T, unsigned D>
const T dot(const Vector<T, D>& lhs, const Vector<T, D>& rhs) noexcept {
    T result = 0;
    for (unsigned i = 0; i < D; i++) result += lhs[i] * rhs[i];
    return result;
}

#if defined(USE_ISPC)
template <IspcSpeedable T, unsigned D>
const T dot(const Vector<T, D>& lhs, const Vector<T, D>& rhs) {
    return ispc::vector_dot(lhs, rhs, D);
}
#endif  // USE_ISPC

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
Vector<T, D> absolute(const Vector<T, D>& a) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::abs(a[i]);
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

}  // namespace hitagi::math
