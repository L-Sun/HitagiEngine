#pragma once
#include <cassert>
#include "vector.hpp"

namespace hitagi::math {

template <typename T, unsigned D>
struct Matrix {
    using RowVec = Vector<T, D>;
    std::array<RowVec, D> data;

    Matrix()              = default;
    Matrix(const Matrix&) = default;
    Matrix(Matrix&&)      = default;
    Matrix& operator=(const Matrix&) = default;
    Matrix& operator=(Matrix&&) = default;

    explicit Matrix(const T num) {
        std::fill_n(&data[0][0], D * D, static_cast<T>(0));
        for (size_t i = 0; i < D; i++)
            data[i][i] = num;
    }
    Matrix(std::initializer_list<RowVec>&& l) { std::move(l.begin(), l.end(), data.begin()); }
    Matrix(std::array<RowVec, D> a) : data(a) {}

    Vector<T, D>&       operator[](unsigned row) { return data[row]; }
    const Vector<T, D>& operator[](unsigned row) const { return data[row]; }

    // TODO make a reference so that we can remove the `const`
    const Vector<T, D> col(unsigned index) const {
        Vector<T, D> result;
        for (unsigned i = 0; i < D; i++)
            result[i] = data[i][index];
        return result;
    }

    operator T*() noexcept { return &data[0][0]; }
    operator const T*() const noexcept { return static_cast<const T*>(&data[0][0]); }

    friend std::ostream& operator<<(std::ostream& out, const Matrix& mat) {
        out << "[\n";
        for (auto&& row : mat.data) {
            out << " " << row << ",\n";
        }
        return out << "]" << std::flush;
    }

    // Matrix Operation
    Matrix operator+(const Matrix& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = data[row] + rhs[row];
        return result;
    }

    Matrix operator-() const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = -data[row];
        return result;
    }
    Matrix operator-(const Matrix& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result.data[row] = data[row] - rhs[row];
        return result;
    }
    Matrix operator*(const Matrix& rhs) const noexcept {
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

    Vector<T, D> operator*(const Vector<T, D>& rhs) const noexcept {
        Vector<T, D> result;
        for (unsigned row = 0; row < D; row++) result[row] = dot(data[row], rhs);
        return result;
    }
    Matrix operator*(const T& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result[row] = data[row] * rhs;
        return result;
    }
    friend Matrix operator*(const T& lhs, const Matrix& rhs) noexcept { return rhs * lhs; }

    Matrix operator/(const T& rhs) const noexcept {
        Matrix result;
        for (unsigned row = 0; row < D; row++) result[row] = data[row] / rhs;
        return result;
    }

    Matrix& operator+=(const Matrix& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] += rhs[row];
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] -= rhs[row];
        return *this;
    }
    Matrix& operator*=(const T& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] *= rhs;
        return *this;
    }
    Matrix& operator/=(const T& rhs) noexcept {
        for (unsigned row = 0; row < D; row++) data[row] /= rhs;
        return *this;
    }
#if defined(USE_ISPC)
    explicit Matrix(const T num) requires IspcSpeedable<T> {
        ispc::zero(*this, D * D);
        for (size_t i = 0; i < D; i++)
            data[i][i] = num;
    }

    Matrix operator+(const Matrix& rhs) const noexcept requires IspcSpeedable<T> {
        Matrix result{};
        ispc::vector_add(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator-() const noexcept requires IspcSpeedable<T> {
        Matrix result;
        ispc::vector_inverse(*this, result, D * D);
        return result;
    }
    Matrix operator-(const Matrix& rhs) const noexcept requires IspcSpeedable<T> {
        Matrix result{};
        ispc::vector_sub(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator*(const T& rhs) const noexcept requires IspcSpeedable<T> {
        Matrix result{};
        ispc::vector_mult(*this, rhs, result, D * D);
        return result;
    }
    Matrix operator/(const T& rhs) const noexcept requires IspcSpeedable<T> {
        Matrix result{};
        ispc::vector_div(*this, rhs, result, D * D);
        return result;
    }

    Matrix& operator+=(const Matrix& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_add_assgin(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator-=(const Matrix& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_sub_assgin(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator*=(const T& rhs) noexcept requires IspcSpeedable<T> {
        ispc::vector_mult_assgin(*this, rhs, D * D);
        return *this;
    }
    Matrix& operator/=(const T& rhs) noexcept requires IspcSpeedable<T> {
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
    Matrix result(0);
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
        std::cerr << "[Math] Warning: the matrix is singular! Function will return a identity matrix!" << std::endl;
        return Matrix<T, 3>(static_cast<T>(1));
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
        std::cerr << "[Math] Warning: the matrix is singular! Function will return a identity matrix!" << std::endl;
        return Matrix<T, 4>(static_cast<T>(1));
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