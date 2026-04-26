module;
#if defined(USE_ISPC)
#include "ispc_math.hpp"
#endif  // USE_ISPC

export module math;
import std;
import utils;

export namespace hitagi::math {

template <typename T, unsigned D>
struct Vector;

template <typename T, unsigned D, unsigned... Indices>
class Swizzle {
public:
    std::array<T, D> data;

    Swizzle& operator=(const T& v) {
        constexpr std::array<unsigned, sizeof...(Indices)> indexs = {Indices...};
        for (auto&& i : indexs) data[i] = v;
        return *this;
    }
    Swizzle& operator=(const std::initializer_list<T>& l) {
        constexpr std::array<unsigned, sizeof...(Indices)> indexs = {Indices...};
        unsigned                                           i      = 0;
        for (auto&& e : l) data[indexs[i++]] = e;
        return *this;
    }
    Swizzle& operator=(const Vector<T, sizeof...(Indices)>& v) {
        constexpr std::array<unsigned, sizeof...(Indices)> indexs = {Indices...};
        for (auto&& i : indexs) data[i] = v[i];
        return *this;
    }

    operator Vector<T, sizeof...(Indices)>() const noexcept { return Vector<T, sizeof...(Indices)>{data[Indices]...}; }
};

template <typename T, unsigned D>
struct BaseVector {
    union {
        std::array<T, D> data;
    };
    BaseVector() = default;
    constexpr BaseVector(std::array<T, D> a) : data{a} {}
};

template <typename T>
struct BaseVector<T, 2> {
    // clang-format off
    union {
        std::array<T, 2> data;
        struct { T x, y; };
        struct { T u, v; };
        Swizzle<T, 2, 0, 1> xy, uv;
        Swizzle<T, 2, 1, 0> yx, vu;
    };
    // clang-format on
    BaseVector() = default;
    constexpr BaseVector(std::array<T, 2> a) : data{a} {}
    constexpr BaseVector(const T& x, const T& y) : data{x, y} {}
};

template <typename T>
struct BaseVector<T, 3> {
    // clang-format off
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
    // clang-format on
    BaseVector() = default;
    constexpr BaseVector(std::array<T, 3> a) : data{a} {}
    constexpr BaseVector(const T& x, const T& y, const T& z) : data{x, y, z} {}
};

template <typename T>
struct BaseVector<T, 4> {
    // clang-format off
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
    // clang-format on
    BaseVector() = default;
    constexpr BaseVector(std::array<T, 4> a) : data{a} {}
    constexpr BaseVector(const T& x, const T& y, const T& z, const T& w) : data{x, y, z, w} {}
};

template <typename T, unsigned D>
struct Vector : public BaseVector<T, D> {
    using BaseVector<T, D>::data;
    using BaseVector<T, D>::BaseVector;
    using value_type = T;

    Vector(const Vector&)                = default;
    Vector(Vector&&) noexcept            = default;
    Vector& operator=(const Vector&)     = default;
    Vector& operator=(Vector&&) noexcept = default;

    constexpr explicit Vector(T num) : BaseVector<T, D>(utils::create_array<T, D>(std::forward<T>(num))) {}

    Vector(const Vector<T, D - 1>& v, const T& num) {
        std::copy_n(v.data.begin(), D - 1, data.begin());
        data[D - 1] = num;
    }

    template <typename TT>
    Vector(const TT* p) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] = static_cast<T>(*p++);
    }

    constexpr static unsigned size() { return D; }

    constexpr const T norm() const noexcept {
        T result = 0;
        for (auto&& val : data) result += val * val;
        return std::sqrt(result);
    }

    operator T*() { return data.data(); }
    operator const T*() const { return data.data(); }

    T&       operator[](unsigned index) noexcept { return data[index]; }
    const T& operator[](unsigned index) const noexcept { return data[index]; }

    friend std::ostream& operator<<(std::ostream& out, const Vector& v) {
        return out << std::format("{}", v) << std::flush;
    }

    constexpr Vector& operator=(T num) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] = num;
        return *this;
    }

    constexpr Vector operator+(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] + rhs[i];
        return result;
    }

    constexpr Vector operator-() const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = -data[i];
        return result;
    }
    constexpr Vector operator-(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] - rhs[i];
        return result;
    }

    constexpr Vector operator*(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] * rhs[i];
        return result;
    }
    constexpr Vector operator*(const T& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] * rhs;
        return result;
    }
    friend Vector    operator*(const T& lhs, const Vector& rhs) noexcept { return rhs * lhs; }
    constexpr Vector operator/(const T& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] / rhs;
        return result;
    }
    constexpr Vector operator/(const Vector& rhs) const noexcept {
        Vector result;
        for (unsigned i = 0; i < D; i++) result.data[i] = data[i] / rhs[i];
        return result;
    }
    constexpr Vector& operator+=(const Vector& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] += rhs[i];
        return *this;
    }
    constexpr Vector& operator-=(const Vector& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] -= rhs[i];
        return *this;
    }
    constexpr Vector& operator*=(const T& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] *= rhs;
        return *this;
    }
    constexpr Vector& operator/=(const T& rhs) noexcept {
        for (unsigned i = 0; i < D; i++) data[i] /= rhs;
        return *this;
    }
    constexpr bool operator==(const Vector& rhs) const noexcept {
        return data == rhs.data;
    }
    constexpr bool operator!=(const Vector& rhs) const noexcept {
        return data != rhs.data;
    }

#if defined(USE_ISPC)
    Vector operator+(const Vector& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_add(*this, rhs, result, D);
        return result;
    }

    Vector operator-() const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_inverse(*this, result, D);
        return result;
    }
    Vector operator-(const Vector& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_sub(*this, rhs, result, D);
        return result;
    }
    Vector operator*(const Vector& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_mult_vector(*this, rhs, result, D);
        return result;
    }
    Vector operator*(const T& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_mult(*this, rhs, result, D);
        return result;
    }
    Vector operator/(const T& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_div(*this, rhs, result, D);
        return result;
    }
    Vector operator/(const Vector& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Vector result;
        ispc::vector_div_vector(*this, rhs, result, D);
        return result;
    }
    Vector& operator+=(const Vector& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_add_assign(*this, rhs, D);
        return *this;
    }
    Vector& operator-=(const Vector& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_sub_assign(*this, rhs, D);
        return *this;
    }
    Vector& operator*=(const T& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_mult_assign(*this, rhs, D);
        return *this;
    }
    Vector& operator/=(const T& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_div_assign(*this, rhs, D);
        return *this;
    }
#endif  // USE_ISPC
};

template <typename T>
struct Quaternion : public Vector<T, 4> {
    using Vector<T, 4>::Vector;
    using Vector<T, 4>::data;

    constexpr static Quaternion identity() noexcept { return {0, 0, 0, 1}; }

    constexpr Quaternion operator*(const Quaternion& rhs) const noexcept {
        return {
            data[0] * rhs.data[3] + data[3] * rhs.data[0] + data[1] * rhs.data[2] - data[2] * rhs.data[1],
            data[1] * rhs.data[3] + data[3] * rhs.data[1] + data[2] * rhs.data[0] - data[0] * rhs.data[2],
            data[2] * rhs.data[3] + data[3] * rhs.data[2] + data[0] * rhs.data[1] - data[1] * rhs.data[0],
            data[3] * rhs.data[3] - data[0] * rhs.data[0] - data[1] * rhs.data[1] - data[2] * rhs.data[2],
        };
    }

    constexpr Quaternion invert(const Quaternion& q) {
        Quaternion result = q;

        T length  = q.norm();
        T length2 = length * length;

        if (length2 != 0.0) {
            float invLength = 1.0f / length2;

            result.x *= -invLength;
            result.y *= -invLength;
            result.z *= -invLength;
            result.w *= invLength;
        }

        return result;
    }
};

struct Color : public Vector<float, 4> {
    using Vector<float, 4>::Vector;
    using Vector<float, 4>::data;

    constexpr static Color Black() noexcept { return {0, 0, 0, 1}; }
    constexpr static Color White() noexcept { return {1, 1, 1, 1}; }
    constexpr static Color Red() noexcept { return {1, 0, 0, 1}; }
    constexpr static Color Green() noexcept { return {0, 1, 0, 1}; }
    constexpr static Color Blue() noexcept { return {0, 0, 1, 1}; }
    constexpr static Color Yellow() noexcept { return {1, 1, 0, 1}; }
    constexpr static Color Cyan() noexcept { return {0, 1, 1, 1}; }
    constexpr static Color Magenta() noexcept { return {1, 0, 1, 1}; }
};

using vec2f = Vector<float, 2>;
using vec3f = Vector<float, 3>;
using vec4f = Vector<float, 4>;

using vec2d = Vector<double, 2>;
using vec3d = Vector<double, 3>;
using vec4d = Vector<double, 4>;

using quatf = Quaternion<float>;
using quatd = Quaternion<double>;

using R8G8B8A8Unorm = Vector<std::uint8_t, 4>;

using vec2i = Vector<std::int32_t, 2>;
using vec3i = Vector<std::int32_t, 3>;
using vec4i = Vector<std::int32_t, 4>;

using vec2u = Vector<std::uint32_t, 2>;
using vec3u = Vector<std::uint32_t, 3>;
using vec4u = Vector<std::uint32_t, 4>;

template <typename T, unsigned D>
constexpr const T dot(const Vector<T, D>& lhs, const Vector<T, D>& rhs) noexcept {
    T result = 0;
    for (unsigned i = 0; i < D; i++) result += lhs[i] * rhs[i];
    return result;
}

#if defined(USE_ISPC)
template <IspcAccelerable T, unsigned D>
const T dot(const Vector<T, D>& lhs, const Vector<T, D>& rhs) {
    return ispc::vector_dot(lhs, rhs, D);
}
#endif  // USE_ISPC

template <typename T, unsigned D>
constexpr Vector<T, D> normalize(const Vector<T, D>& v) {
    return v / v.norm();
}

template <typename T>
constexpr Vector<T, 3> cross(const Vector<T, 3>& v1, const Vector<T, 3>& v2) {
    return {v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x};
}

template <typename T, unsigned D>
constexpr Vector<T, D> absolute(const Vector<T, D>& a) {
    Vector<T, D> res;
    for (unsigned i = 0; i < D; i++) res[i] = std::abs(a[i]);
    return res;
}

template <typename T, unsigned D>
constexpr T max(const Vector<T, D>& v) {
    return *std::max_element(v.data.begin(), v.data.end());
}

template <typename T, unsigned D>
constexpr Vector<T, D> max(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b);
    return res;
}

template <typename T, unsigned D>
constexpr Vector<T, D> max(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::max(a[i], b[i]);
    return res;
}

template <typename T, unsigned D>
constexpr T min(const Vector<T, D>& v) {
    return *std::min_element(v.data.begin(), v.data.end());
}

template <typename T, unsigned D>
constexpr Vector<T, D> min(const Vector<T, D>& a, const T& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b);
    return res;
}

template <typename T, unsigned D>
constexpr Vector<T, D> min(const Vector<T, D>& a, const Vector<T, D>& b) {
    Vector<T, D> res{};
    for (unsigned i = 0; i < D; i++) res[i] = std::min(a[i], b[i]);
    return res;
}

template <typename T, unsigned D>
constexpr unsigned max_index(const Vector<T, D>& v) {
    return std::distance(v.data.begin(), std::max_element(v.data.begin(), v.data.end()));
}

template <typename T, unsigned D>
constexpr unsigned min_index(const Vector<T, D>& v) {
    return std::distance(v.data.begin(), std::min_element(v.data.begin(), v.data.end()));
}

}  // namespace hitagi::math

export template <typename T, unsigned D>
struct std::formatter<hitagi::math::Vector<T, D>> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto           format(const hitagi::math::Vector<T, D>& v, std::format_context& ctx) const {
        auto out = ctx.out();
        out      = std::format_to(out, "[");
        for (unsigned i = 0; i < D; ++i) {
            if (i > 0) out = std::format_to(out, ", ");
            out = std::format_to(out, "{}", v.data[i]);
        }
        out = std::format_to(out, "]");
        return out;
    }
};

export namespace hitagi::math {

template <typename T, unsigned D>
struct Matrix {
    using value_type = T;
    using RowVec     = Vector<T, D>;
    std::array<RowVec, D> data;

    Matrix()                             = default;
    Matrix(const Matrix&)                = default;
    Matrix(Matrix&&) noexcept            = default;
    Matrix& operator=(const Matrix&)     = default;
    Matrix& operator=(Matrix&&) noexcept = default;

    constexpr static Matrix zero() noexcept {
        return {utils::create_array<RowVec, D>(RowVec{0})};
    }

    constexpr static Matrix identity() noexcept {
        constexpr auto init_row = []<std::size_t I, std::size_t... J>(std::integral_constant<std::size_t, I>, std::index_sequence<J...>) -> RowVec {
            return RowVec{{static_cast<T>(I == J ? 1 : 0)...}};
        };
        return [init_row]<std::size_t... I>(std::index_sequence<I...>) {
            return Matrix(std::array<RowVec, D>{init_row(std::integral_constant<std::size_t, I>{}, std::make_index_sequence<D>{})...});
        }(std::make_index_sequence<D>{});
    }

    constexpr Matrix(std::initializer_list<RowVec> l) { std::move(l.begin(), l.end(), data.begin()); }
    constexpr Matrix(std::array<RowVec, D> a) : data{a} {}

    template <typename TT>
    Matrix(const TT* p) noexcept {
        for (unsigned i = 0; i < D; i++)
            for (unsigned j = 0; j < D; j++)
                data[i][j] = static_cast<T>(*p++);
    }

    Vector<T, D>&       operator[](unsigned row) { return data[row]; }
    const Vector<T, D>& operator[](unsigned row) const { return data[row]; }

    // TODO make a reference so that we can remove the `const`
    constexpr const Vector<T, D> col(unsigned index) const {
        Vector<T, D> result;
        for (unsigned i = 0; i < D; i++)
            result[i] = data[i][index];
        return result;
    }

    operator T*() noexcept { return &data[0][0]; }
    operator const T*() const noexcept { return static_cast<const T*>(&data[0][0]); }

    friend std::ostream& operator<<(std::ostream& out, const Matrix& mat) {
        return out << std::format("\n{}\n", mat);
    }

    constexpr bool operator==(const Matrix& rhs) const noexcept {
        for (unsigned row = 0; row < D; row++)
            if (data[row] != rhs[row])
                return false;
        return true;
    }

    // Matrix Operation
    constexpr Matrix operator+(const Matrix& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = data[row] + rhs[row];
        return result;
    }

    constexpr Matrix operator-() const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = -data[row];
        return result;
    }
    constexpr Matrix operator-(const Matrix& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = data[row] - rhs[row];
        return result;
    }
    constexpr Matrix operator*(const Matrix& rhs) const noexcept {
        Matrix       result{};
        Vector<T, D> col_vec;
        for (unsigned col = 0; col < D; col++) {
            for (unsigned i = 0; i < D; i++) col_vec[i] = rhs[i][col];
            for (unsigned row = 0; row < D; row++) {
                result[row][col] = dot(data[row], col_vec);
            }
        }
        return result;
    }

    constexpr Vector<T, D> operator*(const Vector<T, D>& rhs) const noexcept {
        Vector<T, D> result;
        for (unsigned row = 0; row < D; row++) result[row] = dot(data[row], rhs);
        return result;
    }
    constexpr Matrix operator*(const T& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result[row] = data[row] * rhs;
        return result;
    }
    constexpr friend Matrix operator*(const T& lhs, const Matrix& rhs) noexcept { return rhs * lhs; }

    constexpr Matrix operator/(const T& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result[row] = data[row] / rhs;
        return result;
    }

    constexpr Matrix& operator+=(const Matrix& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] += rhs[row];
        return *this;
    }
    constexpr Matrix& operator-=(const Matrix& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] -= rhs[row];
        return *this;
    }
    constexpr Matrix& operator*=(const T& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] *= rhs;
        return *this;
    }
    constexpr Matrix& operator/=(const T& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] /= rhs;
        return *this;
    }
#if defined(USE_ISPC)
    Matrix operator+(const Matrix& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Matrix result{};
        ispc::vector_add(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator-() const noexcept
        requires IspcAccelerable<T>
    {
        Matrix result;
        ispc::vector_inverse(*this, result, D * D);
        return result;
    }
    Matrix operator-(const Matrix& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Matrix result{};
        ispc::vector_sub(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator*(const T& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Matrix result{};
        ispc::vector_mult(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator/(const T& rhs) const noexcept
        requires IspcAccelerable<T>
    {
        Matrix result{};
        ispc::vector_div(*this, rhs, result, D * D);
        return result;
    }

    Matrix& operator+=(const Matrix& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_add_assign(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_sub_assign(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator*=(const T& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_mult_assign(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator/=(const T& rhs) noexcept
        requires IspcAccelerable<T>
    {
        ispc::vector_div_assign(*this, rhs, D * D);
        return *this;
    }
#endif  // USE_ISPC
};

using mat3f = Matrix<float, 3>;
using mat4f = Matrix<float, 4>;
using mat8f = Matrix<float, 8>;

using mat3d = Matrix<double, 3>;
using mat4d = Matrix<double, 4>;
using mat8d = Matrix<double, 8>;

template <typename T, unsigned D>
Matrix<T, D> mul_by_element(const Matrix<T, D>& m1, const Matrix<T, D>& m2) {
    Matrix<T, D> result(0);
    for (unsigned row = 0; row < D; row++) result[row] = m1[row] * m2[row];
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
        std::print("[Math] Warning: the matrix is singular! Function will return a identity matrix!\n");
        return Matrix<T, 3>::identity();
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
        std::print("[Math] Warning: the matrix is singular! Function will return a identity matrix!\n");
        return Matrix<T, 4>::identity();
    }
    T inv_det = static_cast<T>(1) / det;

    for (unsigned i = 0; i < 16; i++) static_cast<T*>(res)[i] = inv[i] * inv_det;
    return res;
}

template <typename T, unsigned D>
void exchange_yz(Matrix<T, D>& matrix) {
    std::swap(matrix.data[1], matrix.data[2]);
}

template <typename T, unsigned D1, unsigned D2>
void shrink(Matrix<T, D1>& mat1, const Matrix<T, D2>& mat2) {
    static_assert(D1 < D2, "[Error] Target matrix order must smaller than source matrix order!");

    for (unsigned row = 0; row < D1; row++)
        for (unsigned col = 0; col < D1; col++) mat1[row][col] = mat2[row][col];
}

template <typename T, unsigned D>
Matrix<T, D> absolute(const Matrix<T, D>& a) {
    Matrix<T, D> res;
    for (unsigned row = 0; row < D; row++)
        for (unsigned col = 0; col < D; col++) res[row][col] = std::abs(a[row][col]);
    return res;
}

}  // namespace hitagi::math

export template <typename T, unsigned D>
struct std::formatter<hitagi::math::Matrix<T, D>> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto           format(const hitagi::math::Matrix<T, D>& m, std::format_context& ctx) const {
        auto out = ctx.out();
        for (unsigned i = 0; i < D; ++i) {
            if (i > 0) out = std::format_to(out, ",\n");
            out = std::format_to(out, "{}", m.data[i]);
        }
        return out;
    }
};

export namespace hitagi::math {

template <typename T>
struct AABB {
    Vector<T, 3> min_point{std::numeric_limits<T>::max()};
    Vector<T, 3> max_point{std::numeric_limits<T>::lowest()};

    constexpr bool Valid() const noexcept {
        return min_point.x <= max_point.x &&
               min_point.y <= max_point.y &&
               min_point.z <= max_point.z;
    }

    constexpr void Expand(const Vector<T, 3>& point) noexcept {
        for (unsigned i = 0; i < 3; ++i) {
            if (point[i] < min_point[i]) min_point[i] = point[i];
            if (point[i] > max_point[i]) max_point[i] = point[i];
        }
    }

    constexpr auto Center() const noexcept -> Vector<T, 3> {
        return (min_point + max_point) * static_cast<T>(0.5);
    }

    constexpr auto Extents() const noexcept -> Vector<T, 3> {
        return (max_point - min_point) * static_cast<T>(0.5);
    }
};

using AABBf = AABB<float>;

struct Frustum {
    std::array<vec4f, 6> planes;

    enum PlaneIndex : unsigned { Left = 0, Right, Bottom, Top, Near, Far };
};

// Gribb-Hartmann frustum plane extraction from a row-major view-projection matrix
// where clip = M * v (column vector convention).
// Each plane (a,b,c,d) satisfies ax + by + cz + d >= 0 for points inside.
inline auto extract_frustum(const mat4f& vp) noexcept -> Frustum {
    Frustum f;
    auto row = [&](unsigned i) -> vec4f { return vp[i]; };

    vec4f r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

    auto normalize_plane = [](vec4f p) -> vec4f {
        float len = vec3f(p.xyz).norm();
        return (len > 1e-8f) ? p / len : p;
    };

    f.planes[Frustum::Left]   = normalize_plane({r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w});
    f.planes[Frustum::Right]  = normalize_plane({r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w});
    f.planes[Frustum::Bottom] = normalize_plane({r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w});
    f.planes[Frustum::Top]    = normalize_plane({r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w});
    f.planes[Frustum::Near]   = normalize_plane({r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w});
    f.planes[Frustum::Far]    = normalize_plane({r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w});
    return f;
}

// Test an AABB against 6 frustum planes.
// Returns true if the AABB is at least partially inside the frustum.
inline bool is_aabb_visible(const Frustum& frustum, const AABBf& aabb) noexcept {
    for (const auto& plane : frustum.planes) {
        vec3f normal{plane.x, plane.y, plane.z};
        // p-vertex: the AABB corner most aligned with the plane normal
        vec3f p{
            normal.x >= 0 ? aabb.max_point.x : aabb.min_point.x,
            normal.y >= 0 ? aabb.max_point.y : aabb.min_point.y,
            normal.z >= 0 ? aabb.max_point.z : aabb.min_point.z,
        };
        if (dot(normal, p) + plane.w < 0) return false;
    }
    return true;
}

// Transform an AABB by a 4x4 matrix, producing a new world-space AABB.
// Uses the Arvo method for tight transformed bounds.
inline auto transform_aabb(const mat4f& m, const AABBf& aabb) noexcept -> AABBf {
    vec3f translation{m[0][3], m[1][3], m[2][3]};
    AABBf result{.min_point = translation, .max_point = translation};

    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            float a = m[i][j] * aabb.min_point[j];
            float b = m[i][j] * aabb.max_point[j];
            result.min_point[i] += std::min(a, b);
            result.max_point[i] += std::max(a, b);
        }
    }
    return result;
}



template <typename T>
constexpr T deg2rad(T angle) {
    return angle / 180.0 * std::numbers::pi;
}

template <typename T>
constexpr T rad2deg(T radians) {
    return radians * 180.0 * std::numbers::inv_pi;
}

constexpr auto to_hex(const Color& v) noexcept -> Vector<std::uint8_t, 4> {
    return {
        static_cast<std::uint8_t>(std::round(v[0] * 255)),
        static_cast<std::uint8_t>(std::round(v[1] * 255)),
        static_cast<std::uint8_t>(std::round(v[2] * 255)),
        static_cast<std::uint8_t>(std::round(v[3] * 255)),
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
constexpr auto axis_angle_to_quaternion(const Vector<T, 3>& axis, T angle) noexcept -> Quaternion<T> {
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

// The _deg literal must be in global namespace
export constexpr double operator""_deg(long double angle) {
    return angle / 180.0 * std::numbers::pi;
}
