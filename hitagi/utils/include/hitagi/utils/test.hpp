#pragma once
#include <hitagi/math/vector.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <benchmark/benchmark.h>
#include <spdlog/spdlog.h>

#include <ranges>
#include <iterator>

namespace hitagi::testing {

template <typename T, unsigned N>
void vector_near(const math::Vector<T, N>& lhs, const math::Vector<T, N>& rhs, double abs_error) {
    for (unsigned index = 0; index < N; index++) {
        EXPECT_NEAR(lhs[index], rhs[index], abs_error) << " at index " << index;
    }
}
}  // namespace hitagi::testing