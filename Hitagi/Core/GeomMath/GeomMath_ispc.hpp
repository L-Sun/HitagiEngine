#pragma once
#include <cmath>

#include <fmt/format.h>

namespace ispc { /* namespace */
extern "C" {
void Absolute(float* result, const float* a, const uint32_t count);
void AddByElement(const float* a, const float* b, float* result, uint32_t count);
void AddByNum(const float* a, const float b, float* result, uint32_t count);
void IncreaceByElement(float* traget, const float* src, uint32_t count);
void IncreaceByNum(float* traget, const float num, uint32_t count);
void DCT(const float* in_matrix, float* out_matrix);
void IDCT(const float* in_matrix, float* out_matrix);
void DivNum(const float* a, const float b, float* result, uint32_t count);
void DivSelfByNum(float* src, const float num, uint32_t count);
void Identity(float* mat, float v, uint32_t order);
bool Inverse(const float* mat, float* ret, uint32_t n);
bool InverseMatrix4X4f(float* matrix);
void Max(float* res, const float* a, float b, const uint32_t count);
void Min(float* res, const float* a, float b, const uint32_t count);
void MatMul(const float* a, const float* b, float* result, uint32_t rows, uint32_t middle, uint32_t cols);
void MulByElement(const float* a, const float* b, float* result, uint32_t count);
void MulByNum(const float* a, const float b, float* result, uint32_t count);
void MulSelfByNum(float* a, const float b, uint32_t count);
void DecreaceByElement(float* target, const float* src, uint32_t count);
void DecreaceByNum(float* target, const float num, uint32_t count);
void Negate(const float* src, float* result, uint32_t count);
void NegateSubByNum(const float num, const float* a, float* result, uint32_t count);
void SubByElement(const float* a, const float* b, float* result, uint32_t count);
void SubByNum(const float* a, const float num, float* result, uint32_t count);
void CrossProduct(const float* a, const float* b, float* result);
void DotProduct(const float* a, const float* b, float* result, uint32_t count);
void Normalize(float* a, uint32_t count);
}
}  // namespace ispc

namespace Hitagi {

template <typename T, int ROWS, int COLS>
struct Matrix;

template <typename T, int D>
struct Vector;

template <typename T, int D, int... Indexs>
class swizzle {
public:
    T data[D];

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
        for (auto&& e : l) data[indexs[i++]] = e;
        return *this;
    }
    swizzle& operator=(const Vector<T, sizeof...(Indexs)>& v) {
        size_t indexs[] = {Indexs...};
        for (size_t i = 0; i < sizeof...(Indexs); i++) {
            data[indexs[i]] = v[i];
        }
        return *this;
    }

    operator Vector<T, sizeof...(Indexs)>() { return Vector<T, sizeof...(Indexs)>({data[Indexs]...}); }
    operator const Vector<T, sizeof...(Indexs)>() const { return Vector<T, sizeof...(Indexs)>({data[Indexs]...}); }
};

// clang-format off
template <typename T, int D>
struct BaseVector {
    union {
        T data[D];
    };
    BaseVector() = default;
};

template <typename T>
struct BaseVector<T, 2> {
    union {
        T data[2];
        struct { T x, y; };
        struct { T u, v; };
        swizzle<T, 2, 0, 1> xy, uv;
        swizzle<T, 2, 1, 0> yx, vu;
    };
    BaseVector() = default;
    BaseVector(const T& x, const T& y) : data{x, y} {}
};

template <typename T>
struct BaseVector<T, 3> {
    union {
        T data[3];
        struct { T x, y, z; };
        struct { T r, g, b; };
        swizzle<T, 3, 0, 1, 2> xyz, rgb;
        swizzle<T, 3, 0, 2, 1> xzy, rbg;
        swizzle<T, 3, 1, 0, 2> yxz, grb;
        swizzle<T, 3, 1, 2, 0> yzx, gbr;
        swizzle<T, 3, 2, 0, 1> zxy, brg;
        swizzle<T, 3, 2, 1, 0> zyx, bgr;
    };
    BaseVector() = default;
    BaseVector(const T& x, const T& y, const T& z) : data{x, y, z} {}
};

template <typename T>
struct BaseVector<T, 4> {
    union {
        T data[4];
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
    BaseVector() = default;
    BaseVector(const T& x, const T& y, const T& z,const T& w) : data{x, y, z, w} {}
};
// clang-format on

template <typename T, int D>
struct Vector : public BaseVector<T, D> {
    using BaseVector<T, D>::data;
    using BaseVector<T, D>::BaseVector;

    Vector() = default;
    Vector(std::initializer_list<T> l) { std::move(l.begin(), l.end(), data); }
    Vector(const T& num) { std::fill(data, data + D, num); }

    template <typename TT>
    Vector(const TT* p) {
        for (size_t i = 0; i < D; i++) {
            data[i] = static_cast<T>(*p++);
        }
    }

    Vector(const Vector& v) { std::copy(v.data, v.data + D, data); }
    Vector(const Vector<T, D - 1>& v, const T& num) {
        std::copy(v.data, v.data + D - 1, data);
        data[D - 1] = num;
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
        return out << fmt::format("({:.2f})", fmt::join(v.data, ", "));
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
    friend const Vector operator+(const T& lhs, const Vector& rhs) { return rhs + lhs; }

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
    friend const Vector operator*(const T& lhs, const Vector& rhs) { return rhs * lhs; }
    const Vector        operator/(const T& rhs) const {
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
    Vector& operator*=(const Matrix<T, D, D>& rhs) { return (*this = *this * rhs); }
    Vector& operator/=(const T& rhs) {
        ispc::DivSelfByNum(data, rhs, D);
        return *this;
    }
};

template <typename T, int ROWS, int COLS>
struct Matrix {
    using col_type = Vector<T, ROWS>;

    col_type data[COLS];

    Matrix() = default;
    Matrix(const T v) { ispc::Identity(&data[0][0], v, ROWS); }
    Matrix(std::initializer_list<Vector<T, COLS>> l) {
        auto p = l.begin();
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                data[j][i] = (*p)[j];
            }
            p++;
        }
    }
    Matrix(const T (&a)[ROWS][COLS]) {
        for (int i = 0; i < ROWS; i++) {
            for (int j = 0; j < COLS; j++) {
                data[j][i] = a[i][j];
            }
        }
    }
    Matrix(const T (&a)[ROWS * COLS]) { std::copy(a, a + ROWS * COLS, &data[0][0]); }
    Matrix(const T* p) { std::copy(p, p + ROWS * COLS, &data[0][0]); }

    T&       operator()(const int& row, const int& col) { return data[col][row]; }
    const T& operator()(const int& row, const int& col) const { return data[col][row]; }

    operator T*() { return &data[0][0]; }
    operator const T*() const { return static_cast<const T*>(&data[0][0]); }

    friend std::ostream& operator<<(std::ostream& out, const Matrix& mat) {
        for (int i = 0; i < ROWS; i++) {
            if (i == 0)
                out << "\n{{";
            else
                out << " {";
            for (unsigned j = 0; j < COLS; j++) {
                out << fmt::format("{:.2f}", mat(i, j));
                if (j != COLS - 1) out << ", ";
            }
            if (i != ROWS - 1)
                out << "},\n";
            else
                out << "}}";
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
    friend const Matrix operator+(T lhs, const Matrix& rhs) { return rhs + lhs; }

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
    const col_type operator*(const col_type& rhs) const {
        col_type result;
        ispc::MatMul(*this, rhs, result, ROWS, COLS, 1);
        return result;
    }
    const Matrix operator*(const T& rhs) const {
        Matrix result;
        ispc::MulByNum(*this, rhs, result, ROWS * COLS);
        return result;
    }
    friend Matrix operator*(const T& lhs, const Matrix& rhs) { return rhs * lhs; }
    const Matrix  operator/(const T& rhs) const {
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

}  // namespace Hitagi
