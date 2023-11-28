#pragma once
#include <concepts>
#include <type_traits>

namespace hitagi::utils {

// clang-format off
template <typename T, typename U>
concept remove_const_pointer_same =
    std::is_pointer_v<T> &&
    std::is_pointer_v<U> &&
    std::same_as<
         std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>,
         std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<U>>>
    >;
// clang-format on

template <typename T>
constexpr bool is_const_reference_v = std::is_reference_v<T> && std::is_const_v<std::remove_reference_t<T>>;

template <typename T>
constexpr bool is_no_const_reference_v = std::is_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>;

template <typename T>
concept no_cvref = (!std::is_reference_v<T>)&&!(std::is_const_v<T>)&&(!std::is_volatile_v<T>);

template <typename...>
constexpr auto is_unique_v = true;

template <typename T, typename... Rest>
constexpr auto is_unique_v<T, Rest...> =
    !std::disjunction_v<std::is_same<T, Rest>...> && is_unique_v<Rest...>;

template <typename... Types>
concept unique_types = is_unique_v<Types...>;

template <typename... Vals>
constexpr bool is_unique_values(Vals... vals) { return true; }

template <typename Val, typename... Vals>
constexpr bool is_unique_values(Val val, Vals... vals) {
    return ((!std::is_same_v<std::remove_cv_t<Val>, std::remove_cv_t<Vals>> || val != vals) && ...) && is_unique_values(vals...);
}

template <typename Ty1, typename Ty2>
concept not_same_as = (!std::is_same_v<Ty1, Ty2>);

template <typename T, typename... U>
concept any_of = (std::same_as<T, U> || ...);

template <typename T, typename... U>
concept no_in = (!any_of<T, U...>);

}  // namespace hitagi::utils