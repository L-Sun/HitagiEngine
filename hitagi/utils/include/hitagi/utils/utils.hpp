#pragma once
#include <cstddef>
#include <string_view>
#include <fmt/format.h>

constexpr std::size_t operator""_kB(unsigned long long val) { return val << 10; }

namespace hitagi::utils {
constexpr std::size_t align(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

[[noreturn]] inline void unreachable() {
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(__GNUC__)  // GCC, Clang, ICC
    __builtin_unreachable();
#elif defined(_MSC_VER)  // MSVC
    __assume(false);
#endif
}

constexpr inline auto add_parentheses(std::string_view str) noexcept {
    return str.empty() ? "" : fmt::format("({})", str);
}

}  // namespace hitagi::utils