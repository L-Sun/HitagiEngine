#pragma once
#include <hitagi/utils/hash.hpp>

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

}  // namespace hitagi::utils

namespace std {
template <>
struct hash<hitagi::utils::TypeID> {
    constexpr std::size_t operator()(const hitagi::utils::TypeID& entity) const {
        return entity.GetValue();
    }
};
}  // namespace std