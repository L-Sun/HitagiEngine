#pragma once
#include <cstddef>

namespace hitagi::utils {
constexpr std::size_t align(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

}  // namespace hitagi::utils