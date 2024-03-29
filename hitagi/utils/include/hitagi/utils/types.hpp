#pragma once
#include <hitagi/utils/hash.hpp>
#include <hitagi/utils/concepts.hpp>

#include <magic_enum.hpp>

#include <typeinfo>
#include <type_traits>
#include <optional>

namespace hitagi::utils {
class TypeID {
public:
    static constexpr auto InvalidValue() noexcept { return static_cast<std::size_t>(-1); }

    template <typename T>
    static constexpr TypeID Create() noexcept { return TypeID{typeid(T).hash_code()}; }

    constexpr TypeID() noexcept : m_Value{InvalidValue()} {}

    explicit constexpr TypeID(std::size_t value) noexcept : m_Value(value) {}

    explicit constexpr TypeID(std::string_view str) noexcept : m_Value(utils::string_hash(str)) {}

    constexpr auto GetValue() const noexcept { return m_Value; }

    template <typename T>
    bool Is() const noexcept { return m_Value == Create<T>(); }

    constexpr bool Valid() const noexcept { return m_Value != InvalidValue(); }

    explicit constexpr operator bool() const noexcept { return Valid(); }

    constexpr std::strong_ordering operator<=>(const TypeID&) const noexcept = default;

private:
    std::size_t m_Value;
};

struct Window {
    enum struct Type : std::uint8_t {
#ifdef _WIN32
        Win32,
#endif
        SDL2,
    };
    Type  type;
    void* ptr;
};

template <typename T>
using optional_ref = std::optional<std::reference_wrapper<T>>;

template <typename T>
auto make_optional_ref(T& data) -> optional_ref<T> {
    if constexpr (std::is_const_v<T>) {
        return std::make_optional(std::cref(data));
    } else {
        return std::make_optional(std::ref(data));
    }
}

// this impl just for disable static check after pack unfolding
namespace detail {
template <typename T, typename MapItem, typename... MapItems>
struct type_mapper {
    using type = std::conditional_t<std::is_same_v<T, typename MapItem::first_type>, typename MapItem::second_type,
                                    typename type_mapper<T, MapItems...>::type>;
};
template <typename T, typename MapItem>
struct type_mapper<T, MapItem> {
    using type = typename MapItem::second_type;
};

template <auto E, typename MapItem, typename... MapItems>
struct val_type_mapper {
    using type = std::conditional_t<E == MapItem::value, typename MapItem::type, typename val_type_mapper<E, MapItems...>::type>;
};

template <auto E, typename MapItem>
struct val_type_mapper<E, MapItem> {
    using type = typename MapItem::type;
};

}  // namespace detail

template <typename T1, typename T2>
struct type_map_item {
    using first_type  = T1;
    using second_type = T2;
};

template <typename T, typename MapItem, typename... MapItems>
    requires any_of<T, typename MapItem::first_type, typename MapItems::first_type...> && unique_types<typename MapItem::first_type, typename MapItems::first_type...>
struct type_mapper {
    using type = detail::type_mapper<T, MapItem, MapItems...>::type;
};

template <auto E, typename T>
struct val_type_map_item {
    static constexpr auto value = E;
    using type                  = T;
};

template <auto E, typename MapItem, typename... MapItems>
struct val_type_mapper {
    static_assert(
        (std::is_same_v<decltype(E), std::remove_cv_t<decltype(MapItem::value)>> && E == MapItem::value) ||
            ((std::is_same_v<decltype(E), std::remove_cv_t<decltype(MapItems::value)>> && E == MapItems::value) || ...),
        "Value not found in mapping.");
    static_assert(is_unique_values(MapItem::value, MapItems::value...), "Duplicate value found in mapping.");

    using type = detail::val_type_mapper<E, MapItem, MapItems...>::type;
};

// https://stackoverflow.com/a/7943765/6244553
// For generic types, directly use the result of the signature of its 'operator()'
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

// we specialize for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
    // arity is the number of arguments.

    constexpr static std::size_t args_size = sizeof...(Args);

    using return_type = ReturnType;

    using args          = std::tuple<Args...>;
    using no_cvref_args = std::tuple<std::remove_cvref_t<Args>...>;

    template <std::size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };

    template <std::size_t i>
    using arg_t = typename arg<i>::type;

    template <std::size_t i>
    struct no_cvref_arg {
        using type = typename std::tuple_element<i, std::tuple<std::remove_cvref_t<Args>...>>::type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
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

}  // namespace hitagi::utils

namespace std {
template <>
struct hash<hitagi::utils::TypeID> {
    constexpr std::size_t operator()(const hitagi::utils::TypeID& entity) const {
        return entity.GetValue();
    }
};
}  // namespace std