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

    constexpr TypeID(std::string_view str) noexcept : m_Value(utils::string_hash(str)) {}

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
namespace details {
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

}  // namespace details

template <typename T1, typename T2>
struct type_map_item {
    using first_type  = T1;
    using second_type = T2;
};

template <typename T, typename MapItem, typename... MapItems>
    requires any_of<T, typename MapItem::first_type, typename MapItems::first_type...> && unique_types<typename MapItem::first_type, typename MapItems::first_type...>
struct type_mapper {
    using type = details::type_mapper<T, MapItem, MapItems...>::type;
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

    using type = details::val_type_mapper<E, MapItem, MapItems...>::type;
};

}  // namespace hitagi::utils

namespace std {
template <>
struct hash<hitagi::utils::TypeID> {
    constexpr std::size_t operator()(const hitagi::utils::TypeID& entity) const {
        return entity.GetValue();
    }
};
}  // namespace std