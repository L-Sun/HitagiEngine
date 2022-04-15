#pragma once
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>

namespace hitagi::testing {

template <typename T, unsigned D>
void vector_eq(const math::Vector<T, D>& v1, const math::Vector<T, D>& v2, double epsilon = 1E-5) {
    for (size_t i = 0; i < D; i++) {
        EXPECT_NEAR(v1[i], v2[i], epsilon) << "difference at index: " << i;
    }
}

template <typename T, unsigned D>
void matrix_eq(const math::Matrix<T, D>& mat1, const math::Matrix<T, D>& mat2, double epsilon = 1E-5) {
    for (int i = 0; i < D; i++) {
        for (int j = 0; j < D; j++) {
            EXPECT_NEAR(mat1[i][j], mat2[i][j], epsilon) << "difference at index: [" << i << "][" << j << "]";
        }
    }
}
}  // namespace hitagi::testing