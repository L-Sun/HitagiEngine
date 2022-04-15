#pragma once
#include <concepts>

namespace hitagi::utils {

// clang-format off
template <typename T, typename U>
concept remove_const_pointer_same =
    std::is_pointer_v<T> &&
    std::is_pointer_v<U> &&
    std::same_as<
        typename std::add_pointer<typename std::remove_const<typename std::remove_pointer<T>::type>::type>::type,
        typename std::add_pointer<typename std::remove_const<typename std::remove_pointer<U>::type>::type>::type
    >;
// clang-format on

}  // namespace hitagi::utils