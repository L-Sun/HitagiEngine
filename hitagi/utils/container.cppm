export module utils:container;

import std;
import magic_enum;
import :helper;

export namespace hitagi::utils {

// ============================================================================
// array
// ============================================================================

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

// ============================================================================
// aligned_span
// ============================================================================

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


// ============================================================================
// soa
// ============================================================================

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