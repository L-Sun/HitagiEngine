#pragma once
#include <concepts>
#include <type_traits>
#include <tuple>

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

template <typename...>
inline constexpr auto is_unique = std::true_type{};

template <typename T, typename... Rest>
inline constexpr auto is_unique<T, Rest...> = std::bool_constant<
    (!std::is_same_v<T, Rest> && ...) && is_unique<Rest...>>{};

template <typename... Types>
concept unique_types = is_unique<Types...>;

// https://stackoverflow.com/a/7943765/6244553
// For generic types, directly use the result of the signature of its 'operator()'
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

// we specialize for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    // arity is the number of arguments.

    constexpr static std::size_t args_size = sizeof...(Args);

    using result_type = ReturnType;

    constexpr static bool unique_parameter_types = is_unique<Args...>;

    using args = std::tuple<Args...>;

    template <std::size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
};

template <typename Func>
concept unique_parameter_types = function_traits<Func>::unique_parameter_types;

}  // namespace hitagi::utils