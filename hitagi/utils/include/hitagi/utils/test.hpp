#pragma once
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <spdlog/spdlog.h>

namespace hitagi::testing {

template <std::floating_point T, unsigned D>
::testing::AssertionResult expect_EXPECT_VEC_EQ(
    const char*               lhs_expr,
    const char*               rhs_expr,
    const math::Vector<T, D>& lhs,
    const math::Vector<T, D>& rhs) {
    for (unsigned i = 0; i < D; i++) {
        if (auto diff = std::abs(lhs[i] - rhs[i]); diff > 1e-5) {
            return ::testing::AssertionFailure()
                   << "The difference between " << lhs_expr << " and " << rhs_expr
                   << "at index " << i << " is "
                   << diff << ", where\n"
                   << lhs_expr << "=" << lhs << ",\n"
                   << rhs_expr << "= " << rhs << ".";
        }
    }

    return ::testing::AssertionSuccess();
}

#define EXPECT_VEC_EQ(vec1, vec2) \
    EXPECT_PRED_FORMAT2(expect_EXPECT_VEC_EQ, vec1, vec2)

template <std::floating_point T, unsigned D>
::testing::AssertionResult expect_matrix_eq(
    const char*               lhs_expr,
    const char*               rhs_expr,
    const math::Matrix<T, D>& lhs,
    const math::Matrix<T, D>& rhs) {
    for (unsigned i = 0; i < D; i++) {
        for (unsigned j = 0; j < D; j++) {
            if (auto diff = std::abs(lhs[i][j] - rhs[i][j]); diff > 1e-5) {
                return ::testing::AssertionFailure()
                       << "The difference between " << lhs_expr << " and " << rhs_expr
                       << "at index (" << i << ", " << j << ") is "
                       << diff << ", where\n"
                       << lhs_expr << "=" << lhs << ",\n"
                       << rhs_expr << "= " << rhs << ".";
            }
        }
    }

    return ::testing::AssertionSuccess();
}

#define EXPECT_MAT_EQ(mat1, mat2) \
    EXPECT_PRED_FORMAT2(expect_matrix_eq, mat1, mat2)

}  // namespace hitagi::testing