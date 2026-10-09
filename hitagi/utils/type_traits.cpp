export module utils:type_traits;
import std;

export namespace hitagi::utils {

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
concept no_cvref = (!std::is_reference_v<T>) && !(std::is_const_v<T>) && (!std::is_volatile_v<T>);

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

template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

// we specialize for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    constexpr static std::size_t args_size = sizeof...(Args);

    using return_type = ReturnType;

    using args          = std::tuple<Args...>;
    using no_cvref_args = std::tuple<std::remove_cvref_t<Args>...>;

    template <std::size_t i>
    struct arg {
        using type = std::tuple_element_t<i, std::tuple<Args...>>;
    };

    template <std::size_t i>
    using arg_t = typename arg<i>::type;

    template <std::size_t i>
    struct no_cvref_arg {
        using type = std::tuple_element_t<i, std::tuple<std::remove_cvref_t<Args>...>>;
    };
};

template <typename T>
struct delay_type {
    using type = T;
};

namespace detail {
template <typename T, std::size_t I, typename... Ts>
constexpr std::size_t first_index_of() {
    if constexpr (I >= sizeof...(Ts)) {
        return I;
    } else if constexpr (std::is_same_v<T, std::tuple_element_t<I, std::tuple<Ts...>>>) {
        return I;
    } else {
        return first_index_of<T, I + 1, Ts...>();
    }
}
};  // namespace detail

template <typename T, typename... Ts>
constexpr std::size_t first_index_of_v = detail::first_index_of<T, 0, Ts...>();

template <typename T>
using remove_const_pointer_t = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>;

namespace detail {
template <std::size_t N, typename... Ts>
constexpr auto tuple_head() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return std::tuple<std::tuple_element_t<I, std::tuple<Ts...>>...>{};
    }(std::make_index_sequence<N>{});
}

template <std::size_t N, typename... Ts>
constexpr auto tuple_tail() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return std::tuple<std::tuple_element_t<I + N, std::tuple<Ts...>>...>{};
    }(std::make_index_sequence<sizeof...(Ts) - N>{});
}

template <std::size_t N, typename... Ts>
using tuple_head_types = decltype(tuple_head<N, Ts...>());

template <std::size_t N, typename... Ts>
using tuple_tail_types = decltype(tuple_tail<N, Ts...>());

}  // namespace detail

template <std::size_t N, typename... Ts>
struct split_types {
    using first  = detail::tuple_head_types<N, Ts...>;
    using second = detail::tuple_tail_types<N, Ts...>;
};

namespace detail {
template <unsigned N, unsigned Offset>
constexpr auto make_index_sequence_with_offset_impl() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return std::index_sequence<(I + Offset)...>{};
    }(std::make_index_sequence<N>{});
};
}  // namespace detail
template <unsigned N, unsigned Offset>
using make_index_sequence_with_offset = decltype(detail::make_index_sequence_with_offset_impl<N, Offset>());

namespace detail {
template <unsigned From, unsigned To>
constexpr auto make_index_sequence_from_to_impl() {
    if constexpr (From >= To) {
        return std::index_sequence<>{};
    } else {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return std::index_sequence<(I + From)...>{};
        }(std::make_index_sequence<To - From>{});
    }
};
}  // namespace detail
template <unsigned From, unsigned To>
using make_index_sequence_from_to = decltype(detail::make_index_sequence_from_to_impl<From, To>());

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace hitagi::utils
