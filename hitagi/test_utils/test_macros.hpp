#pragma once

#include <benchmark/benchmark.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Macros cannot be exported through C++20 modules, so they are provided via this header.
// The template functions they reference are exported from the test_utils module.

#define EXPECT_VEC_EQ(vec1, vec2) \
    EXPECT_PRED_FORMAT2(hitagi::testing::expect_EXPECT_VEC_EQ, vec1, vec2)

#define EXPECT_MAT_EQ(mat1, mat2) \
    EXPECT_PRED_FORMAT2(hitagi::testing::expect_matrix_eq, mat1, mat2)
