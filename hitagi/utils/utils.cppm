module;
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

export module utils;
import std;
import magic_enum;

export constexpr std::size_t operator""_kB(unsigned long long val) { return val << 10; }

export namespace hitagi::utils {

constexpr std::size_t align(size_t x, size_t a) {
    return (x + a - 1) & ~(a - 1);
}

[[noreturn]] inline void unreachable() {
#if defined(__GNUC__)  // GCC, Clang, ICC
    __builtin_unreachable();
#elif defined(_MSC_VER)  // MSVC
    __assume(false);
#endif
}

constexpr inline auto add_parentheses(std::string_view str) noexcept {
    return str.empty() ? std::string{} : std::format("({})", str);
}

template <typename T>
class enable_private_make_shared {
public:
    template <typename... Args>
    static auto Create(Args&&... args) -> std::shared_ptr<T> {
        struct CreateTemp : public T {
            CreateTemp(Args&&... args) : T(std::forward<Args>(args)...) {}
        };
        return std::static_pointer_cast<T>(std::make_shared<CreateTemp>(std::forward<Args>(args)...));
    }
};

template <typename T>
class enable_private_allocate_shared {
public:
    template <typename Alloc, typename... Args>
    static std::shared_ptr<T> Create(Alloc&& alloc, Args&&... args) {
        struct CreateTemp : public T {
            using T::T;
        };
        return std::static_pointer_cast<T>(std::allocate_shared<CreateTemp>(std::forward<Alloc>(alloc), std::forward<Args>(args)...));
    }
};



template <typename T>
constexpr inline std::size_t hash(const T& obj) noexcept {
    return std::hash<T>{}(obj);
}

template <std::size_t N>
constexpr inline std::size_t combine_hash(const std::array<std::size_t, N>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

constexpr inline std::size_t combine_hash(const std::pmr::vector<std::size_t>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

inline std::size_t combine_hash(const std::pmr::set<std::size_t>& hash_values, std::size_t seed = 0) noexcept {
    for (std::size_t hash : hash_values) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) noexcept {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

template <typename T1, typename T2>
constexpr inline std::size_t combine_hash(const T1& a, const T2& b, std::size_t seed = 0) noexcept {
    return combine_hash(seed, a, b);
}

namespace detail {
template <unsigned bytesize>
struct fnv1a_traits;
template <>
struct fnv1a_traits<4> {
    using type                            = std::uint32_t;
    static constexpr std::uint32_t offset = 2166136261;
    static constexpr std::uint32_t prime  = 16777619;
};
template <>
struct fnv1a_traits<8> {
    using type                            = std::uint64_t;
    static constexpr std::uint64_t offset = 14695981039346656037ull;
    static constexpr std::uint64_t prime  = 1099511628211ull;
};
}  // namespace detail

constexpr inline std::size_t string_hash(std::string_view str) noexcept {
    using Traits      = detail::fnv1a_traits<sizeof(std::size_t)>;
    std::size_t value = Traits::offset;
    for (const char& c : str) {
        value = (value ^ static_cast<Traits::type>(c)) * Traits::prime;
    }
    return value;
}

template <typename E>
    requires std::is_enum_v<E>
struct enable_bitmask_operators : public magic_enum::customize::enum_range<E> {};

template <typename E>
concept EnumFlag = enable_bitmask_operators<E>::is_flags;

}  // namespace hitagi::utils

export template <hitagi::utils::EnumFlag E>
constexpr E operator|(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

export template <hitagi::utils::EnumFlag E>
constexpr E& operator|=(E& lhs, const E& rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

export template <hitagi::utils::EnumFlag E>
constexpr E operator&(E lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

export template <hitagi::utils::EnumFlag E>
constexpr E& operator&=(E& lhs, E rhs) {
    using underlying = std::underlying_type_t<E>;
    lhs              = static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

export template <hitagi::utils::EnumFlag E>
constexpr E operator~(E rhs) {
    using underlying = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<underlying>(rhs));
}

// has_flag must come after operator& so it's visible at template definition point
export namespace hitagi::utils {

template <EnumFlag E>
constexpr bool has_flag(E lhs, E rhs) {
    return (lhs & rhs) == rhs;
}

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
        SDL3,
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
    constexpr static std::size_t args_size = sizeof...(Args);

    using return_type = ReturnType;

    using args          = std::tuple<Args...>;
    using no_cvref_args = std::tuple<std::remove_cvref_t<Args>...>;

    template <std::size_t i>
    struct arg {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };

    template <std::size_t i>
    using arg_t = typename arg<i>::type;

    template <std::size_t i>
    struct no_cvref_arg {
        using type = typename std::tuple_element<i, std::tuple<std::remove_cvref_t<Args>...>>::type;
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

class NoImplemented : public std::exception {
public:
    explicit NoImplemented(const char* message = "No Implemented!") : msg(message) {}
    explicit NoImplemented(std::string_view message) : NoImplemented(message.data()) {}
    NoImplemented(NoImplemented const&) noexcept = default;

    NoImplemented& operator=(NoImplemented const&) noexcept = default;
    ~NoImplemented() override                               = default;

    const char* what() const noexcept override { return msg; }

private:
    const char* msg;
};

}  // namespace hitagi::utils

export namespace std {
template <>
struct hash<hitagi::utils::TypeID> {
    constexpr std::size_t operator()(const hitagi::utils::TypeID& entity) const {
        return entity.GetValue();
    }
};
}  // namespace std

export namespace hitagi::utils {

template <typename T, std::size_t N>
constexpr std::array<T, N> create_array(T&& value) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{(static_cast<void>(I), std::forward<T>(value))...};
    }(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N, typename... Args>
constexpr std::array<T, N> create_array_inplace(Args&&... args) {
    static_assert(std::is_constructible_v<T, Args...>, "Can not construct array inplace");

    auto construct_fn = [&](std::size_t i) -> T {
        return T{std::forward<Args>(args)...};
    };

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{{construct_fn(I)...}};
    }(std::make_index_sequence<N>{});
}

template <typename T, typename E>
    requires std::is_enum_v<E>
struct EnumArray : public std::array<T, magic_enum::enum_count<E>()> {
    using array_t = typename std::array<T, magic_enum::enum_count<E>()>;

    T&       operator[](std::size_t index) { return std::array<T, magic_enum::enum_count<E>()>::operator[](index); }
    const T& operator[](std::size_t index) const { return std::array<T, magic_enum::enum_count<E>()>::operator[](index); }
    T&       at(std::size_t index) { return std::array<T, magic_enum::enum_count<E>()>::at(index); }
    const T& at(std::size_t index) const { return std::array<T, magic_enum::enum_count<E>()>::at(index); }

    T&       operator[](const E& e) { return std::array<T, magic_enum::enum_count<E>()>::operator[](magic_enum::enum_integer(e)); }
    const T& operator[](const E& e) const { return std::array<T, magic_enum::enum_count<E>()>::operator[](magic_enum::enum_integer(e)); }
    T&       at(const E& e) { return std::array<T, magic_enum::enum_count<E>()>::at(magic_enum::enum_integer(e)); }
    const T& at(const E& e) const { return std::array<T, magic_enum::enum_count<E>()>::at(magic_enum::enum_integer(e)); }
};

template <typename T, typename E>
    requires std::is_enum_v<E>
constexpr EnumArray<T, E> create_enum_array(T&& initial_value) {
    return {create_array<T, magic_enum::enum_count<E>()>(std::forward<T>(initial_value))};
}

template <typename T>
class AlignedSpan {
public:
    AlignedSpan(std::byte* data, std::size_t element_count, std::size_t element_alignment)
        : m_Data(data), m_ElementCount(element_count), m_ElementAlignment(element_alignment) {}

    inline auto data() const noexcept -> T* { return reinterpret_cast<T*>(m_Data); }
    inline auto size() const noexcept -> std::size_t { return m_ElementCount; }
    inline auto element_alignment() const noexcept -> std::size_t { return m_ElementAlignment; }
    inline auto element_size() const noexcept -> std::size_t { return sizeof(T); }
    inline auto aligned_element_size() const noexcept -> std::size_t { return utils::align(sizeof(T), m_ElementAlignment); }

    inline auto front() noexcept -> T& { return *reinterpret_cast<T*>(m_Data); }

    inline auto back() noexcept -> T& { return *reinterpret_cast<T*>(m_Data + (m_ElementCount - 1) * aligned_element_size()); }

    inline auto operator[](std::size_t index) noexcept -> T& { return *reinterpret_cast<T*>(m_Data + index * aligned_element_size()); }

    class Iterator {
    public:
        // clang-format off
        // std::input_iterator
        using value_type = T;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        inline auto operator*() const -> reference { return (*aligned_span)[index]; }
        inline auto operator-(const Iterator& rhs) const noexcept -> difference_type { return index - rhs.index; }
        inline auto operator++() noexcept -> Iterator& { ++index; return *this; }
        inline auto operator++(int) noexcept -> Iterator { Iterator it(*this); index++; return it; }

        // std::forward_iterator
        Iterator() = default;
        inline bool operator==(const Iterator& rhs) const noexcept { return index == rhs.index; }

        // std::bidirectional_iterator
        inline auto operator--() noexcept -> Iterator& { --index; return *this; }
        inline auto operator--(int) noexcept -> Iterator { Iterator it(*this); index--; return it; }

        // std::random_access_iterator
        inline auto operator<=>(const Iterator& rhs) const noexcept = default;
        inline auto operator+=(difference_type n) noexcept ->Iterator& { index += n; return *this; }
        inline auto operator+(difference_type n) const noexcept -> Iterator { return { aligned_span, index + n }; }
        inline auto operator-=(std::size_t n) noexcept -> Iterator& { index -= n; return *this; }
        inline auto operator-(std::size_t n) const noexcept -> Iterator { return { aligned_span, index - n }; }
        inline auto operator[](std::size_t n) const noexcept -> reference { return *(*this + n); }
        inline friend auto operator+(std::size_t n, const Iterator& rhs) noexcept -> Iterator { return { rhs.ptr, rhs.index + n }; }

        // clang-format on
    private:
        friend AlignedSpan;
        Iterator(AlignedSpan* aligned_span, std::size_t index) : aligned_span(aligned_span), index(index) {}
        AlignedSpan* aligned_span;
        std::size_t  index;
    };
    static_assert(std::random_access_iterator<Iterator>);

    auto begin() noexcept -> Iterator { return {this, 0}; }
    auto end() noexcept -> Iterator { return {this, m_ElementCount}; }

protected:
    std::byte*  m_Data;
    std::size_t m_ElementCount;
    std::size_t m_ElementAlignment;
};

template <typename... Types>
class SoA {
public:
    using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

    constexpr static std::size_t NumTypes = sizeof...(Types);
    template <std::size_t N>
    using TypeAt = typename std::tuple_element_t<N, std::tuple<Types...>>;

    using Structure         = std::tuple<Types...>;
    using StructureRef      = std::tuple<Types&...>;
    using StructureConstRef = std::tuple<const Types&...>;
    using StructureForward  = std::tuple<Types&&...>;

    explicit SoA(const allocator_type& allocator = {})
        : m_Data{std::allocator_arg, allocator},
          m_Allocator(allocator) {}

    allocator_type get_allocator() const noexcept { return m_Allocator; }

    /* Element Access */

    StructureConstRef operator[](std::size_t i) const noexcept {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::tie(std::get<I>(m_Data)[i]...);
        }(std::index_sequence_for<Types...>{});
    }
    StructureRef operator[](std::size_t i) noexcept {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::tie(std::get<I>(m_Data)[i]...);
        }(std::index_sequence_for<Types...>{});
    }

    StructureConstRef at(std::size_t i) const {
        range_check(i);
        return (*this)[i];
    }
    StructureRef at(std::size_t i) {
        range_check(i);
        return (*this)[i];
    }

    StructureConstRef front() const {
        return at(0);
    }
    StructureRef front() {
        return at(0);
    }
    StructureConstRef back() const {
        return at(m_Size - 1);
    }
    StructureRef back() {
        return at(m_Size - 1);
    }

    template <std::size_t T>
    const TypeAt<T>& element_at(std::size_t i) const {
        static_assert(T < sizeof...(Types));
        return std::get<T>(m_Data).at(i);
    }
    template <std::size_t T>
    TypeAt<T>& element_at(std::size_t i) {
        return const_cast<TypeAt<T>&>(const_cast<const SoA*>(this)->element_at<T>(i));
    }

    template <std::size_t T>
    auto& elements() {
        return std::get<T>(m_Data);
    }
    template <std::size_t T>
    const auto& elements() const {
        return std::get<T>(m_Data);
    }

    auto begin() noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::begin(std::ranges::views::zip(e...));
            },
            m_Data);
    }
    auto end() noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::end(std::ranges::views::zip(e...));
            },
            m_Data);
    }
    auto begin() const noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::begin(std::ranges::views::zip(e...));
            },
            m_Data);
    }
    auto end() const noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::end(std::ranges::views::zip(e...));
            },
            m_Data);
    }

    /* Capacity */

    [[nodiscard]] constexpr bool empty() const noexcept { return m_Size == 0; }
    constexpr std::size_t        size() const noexcept { return m_Size; }
    constexpr void               reserve(std::size_t n) {
        std::apply([n](auto&&... e) { (e.reserve(n), ...); }, m_Data);
    }
    constexpr void shrink_to_fit() {
        std::apply([](auto&&... e) { (e.shrink_to_fit(), ...); }, m_Data);
    }

    /* Modifier */

    constexpr void clear() noexcept {
        std::apply([](auto&&... e) { (e.clear(), ...); }, m_Data);
        m_Size = 0;
    }
    constexpr void         push_back(StructureConstRef values);
    constexpr StructureRef emplace_back(Types&&... values);
    constexpr void         pop_back();
    constexpr void         resize(std::size_t count);
    constexpr void         resize(std::size_t count, StructureConstRef values);

private:
    void range_check(std::size_t i) const {
        if (i >= m_Size) {
            throw std::out_of_range(std::format(
                "SoA::range_check: index"
                "(which is {}) >= this->size() "
                "(which is {})",
                i, m_Size));
        }
    }

    std::tuple<std::vector<Types>...> m_Data;
    std::size_t                       m_Size = 0;

    allocator_type m_Allocator;
};

template <typename... Types>
constexpr void SoA<Types...>::push_back(StructureConstRef values) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).push_back(std::forward<const TypeAt<I>&>(std::get<I>(values))), ...);
    }(std::index_sequence_for<Types...>{});

    m_Size++;
}

template <typename... Types>
constexpr auto SoA<Types...>::emplace_back(Types&&... values) -> StructureRef {
    auto result = [&]<std::size_t... I>(std::index_sequence<I...>) -> StructureRef {
        return std::tie(std::get<I>(m_Data).emplace_back(std::forward<TypeAt<I>>(values))...);
    }(std::index_sequence_for<Types...>{});

    m_Size++;

    return result;
}
template <typename... Types>
constexpr void SoA<Types...>::pop_back() {
    std::apply([](auto&&... e) { (e.pop_back(), ...); }, m_Data);
    m_Size--;
}
template <typename... Types>
constexpr void SoA<Types...>::resize(std::size_t count) {
    std::apply([count](auto&&... e) { (e.resize(count), ...); }, m_Data);
    m_Size = count;
}
template <typename... Types>
constexpr void SoA<Types...>::resize(std::size_t count, StructureConstRef values) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).resize(count, std::forward<const TypeAt<I>&>(std::get<I>(values))), ...);
    }(std::index_sequence_for<Types...>{});

    m_Size = count;
}

}  // namespace hitagi::utils

export template <typename... Types>
class std::back_insert_iterator<hitagi::utils::SoA<Types...>> {
public:
    using container_type = hitagi::utils::SoA<Types...>;

    explicit back_insert_iterator(container_type& container)
        : container_(std::addressof(container)) {}

    back_insert_iterator<container_type>& operator=(typename container_type::StructureConstRef value) {
        container_->push_back(value);
        return *this;
    }

    back_insert_iterator<container_type>& operator*() {
        return *this;
    }

    back_insert_iterator<container_type>& operator++() {
        return *this;
    }

    back_insert_iterator<container_type>& operator++(int) {
        return *this;
    }

private:
    container_type* container_;
};

export namespace hitagi::utils {
auto try_create_logger(std::string_view name) -> std::shared_ptr<spdlog::logger> {
    std::string logger_name{name};

    auto logger = spdlog::get(logger_name);
    if (logger == nullptr) {
        logger = spdlog::stdout_color_mt(logger_name);
    }
    return logger;
}


class UUID {
public:
    static auto Create() -> UUID;

    auto operator==(const UUID& other) const -> bool = default;
    auto operator!=(const UUID& other) const -> bool = default;

private:
    std::array<std::byte, 16> m_Data;

    friend struct std::formatter<hitagi::utils::UUID>;
};
}  // namespace hitagi::utils

export template <>
struct std::formatter<hitagi::utils::UUID> : std::formatter<std::string> {
    auto format(const hitagi::utils::UUID& uuid, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                return std::format(
                    "{:02x}{:02x}{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}-"
                    "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                    std::to_integer<std::uint8_t>(uuid.m_Data[I])...);
            }(std::make_index_sequence<16>{}),
            ctx);
    }
};
