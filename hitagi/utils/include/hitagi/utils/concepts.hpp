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
constexpr auto is_unique_v = true;

template <typename T, typename... Rest>
constexpr auto is_unique_v<T, Rest...> =
    !std::disjunction_v<std::is_same<T, Rest>...> && is_unique_v<Rest...>;

template <typename T>
concept NoCVRef = (!std::is_reference_v<T>) && !(std::is_const_v<T>) && (!std::is_volatile_v<T>);

template <typename... Types>
concept UniqueTypes = is_unique_v<Types...>;

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

    constexpr static bool unique_parameter_types = is_unique_v<Args...>;

    using args          = std::tuple<Args...>;
    using no_cvref_args = std::tuple<std::remove_cvref_t<Args>...>;

    template <std::size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
    template <std::size_t i>
    struct no_cvref_arg {
        using type = typename std::tuple_element<i, std::tuple<std::remove_cvref_t<Args>...>>::type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
};

template <typename Func>
concept unique_parameter_types = function_traits<Func>::unique_parameter_types;

template <typename Ty1, typename Ty2>
concept not_same_as = (!std::is_same_v<Ty1, Ty2>);

}  // namespace hitagi::utils