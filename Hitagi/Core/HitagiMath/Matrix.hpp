#pragma once
#include <cassert>
#include "./Vector.hpp"

// Fix fmt join with array<RowVec>
template <typename T, unsigned D, typename Char>
struct fmt::formatter<Hitagi::Vector<T, D>, Char> : fmt::v6::internal::fallback_formatter<Hitagi::Vector<T, D>, Char> {
};

namespace Hitagi {

template <typename T, unsigned D>
struct Matrix {
    using RowVec = Vector<T, D>;
    std::array<RowVec, D> data;

    Matrix() = default;
    explicit Matrix(const T num) {
        for (unsigned row = 0; row < D; row++)
            for (unsigned col = 0; col < D; col++) data[row][col] = row == col ? num : 0;
    }
    Matrix(std::initializer_list<RowVec>&& l) { std::move(l.begin(), l.end(), data.begin()); }
    Matrix(const T (&a)[D][D]) { std::copy(&a[0][0], &a[0][0] + D * D, &data[0][0]); }
    Matrix(const T (&a)[D * D]) { std::copy(&a[0], &a[0] + D * D, &data[0][0]); }

    Vector<T, D>&       operator[](unsigned row) { return data[row]; }
    const Vector<T, D>& operator[](unsigned row) const { return data[row]; }

    operator T*() { return &data[0][0]; }
    operator const T*() const { return static_cast<const T*>(&data[0][0]); }

    friend std::ostream& operator<<(std::ostream& out, const Matrix& mat) {
        return out << fmt::format("[\n  {}\n]", fmt::join(mat.data, ",\n  "));
    }

    // Matrix Operation
    const Matrix operator+(const Matrix& rhs) const {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = data[row] + rhs[row];
        return reuslt;
    }
    const Matrix operator+(const T& rhs) const {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = data[row] + rhs;
        return reuslt;
    }
    friend const Matrix operator+(T lhs, const Matrix& rhs) { return rhs + lhs; }

    const Matrix operator-() const {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = -data[row];
        return reuslt;
    }
    const Matrix operator-(const Matrix& rhs) const {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = data[row] - rhs[row];
        return reuslt;
    }
    const Matrix operator-(const T& rhs) const {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = data[row] - rhs;
        return reuslt;
    }
    friend const Matrix operator-(T lhs, const Matrix& rhs) {
        Matrix reuslt(0);
        for (unsigned row = 0; row < D; row++) reuslt.data[row] = lhs - rhs[row];
        return reuslt;
    }
    const Matrix operator*(const Matrix& rhs) const {
        Matrix       result(static_cast<T>(0));
        Vector<T, D> colVec;
        for (unsigned row = 0; row < D; row++) {
            for (unsigned col = 0; col < D; col++) {
                for (unsigned i = 0; i < D; i++) colVec[i] = rhs[i][col];
                result.data[row][col] = dot(data[row], colVec);
            }
        }
        return result;
    }

    const Vector<T, D> operator*(const Vector<T, D>& rhs) const {
        Vector<T, D> result(static_cast<T>(0));
        for (unsigned row = 0; row < D; row++) result[row] = dot(data[row], rhs);
        return result;
    }
    const Matrix operator*(const T& rhs) const {
        Matrix result(static_cast<T>(0));
        for (unsigned row = 0; row < D; row++) result[row] = data[row] * rhs;
        return result;
    }
    friend const Matrix operator*(const T& lhs, const Matrix& rhs) { return rhs * lhs; }

    const Matrix operator/(const T& rhs) const {
        Matrix result(static_cast<T>(0));
        for (unsigned row = 0; row < D; row++) result[row] = data[row] / rhs;
        return result;
    }

    Matrix& operator+=(const Matrix& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] += rhs[row];
        return *this;
    }
    Matrix& operator+=(const T& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] += rhs;
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] -= rhs[row];
        return *this;
    }
    Matrix& operator-=(const T& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] -= rhs;
        return *this;
    }
    Matrix& operator*=(const T& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] *= rhs;
        return *this;
    }
    Matrix& operator/=(const T& rhs) {
        for (unsigned row = 0; row < D; row++) data[row] /= rhs;
        return *this;
    }
};

using mat3f = Matrix<float, 3>;
using mat4f = Matrix<float, 4>;
using mat8f = Matrix<float, 8>;

using mat3d = Matrix<double, 3>;
using mat4d = Matrix<double, 4>;
using mat8d = Matrix<double, 8>;

template <typename T, unsigned D>
Matrix<T, D> mulByElement(const Matrix<T, D>& m1, const Matrix<T, D>& m2) {
    Matrix result(0);
    for (unsigned row = 0; row < D; row++) result[row] = m1[row] * m2[row];
    return result;
}
}  // namespace Hitagi