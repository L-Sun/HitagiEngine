#include "concepts.hpp"

#include <fmt/format.h>

#include <memory_resource>

namespace hitagi::utils {

template <typename... Types>
class SoA {
public:
    using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

    template <std::size_t N>
    using TypeAt = typename std::tuple_element_t<N, std::tuple<Types...>>;

    using Structure         = std::tuple<Types...>;
    using StructureRef      = std::tuple<Types&...>;
    using StructureConstRef = std::tuple<const Types&...>;
    using StructureForward  = std::tuple<Types&&...>;

    explicit SoA(allocator_type allocator = {})
        : m_Data{std::allocator_arg, allocator},
          m_Allocator(allocator) {}

    allocator_type get_allocator() noexcept { return m_Allocator; }

    /* Element Access */

    StructureConstRef operator[](std::size_t i) const noexcept {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::tie(std::get<I>(m_Data)[i]...);
        }
        (std::index_sequence_for<Types...>{});
    }
    StructureRef operator[](std::size_t i) noexcept {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return std::tie(std::get<I>(m_Data)[i]...);
        }
        (std::index_sequence_for<Types...>{});
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

    /* Iterator */

    template <typename SoAPointer>
    requires remove_const_pointer_same<SoAPointer, SoA*>
    class Iterator {
        friend class SoA;
        SoAPointer  soa;
        std::size_t index;
        Iterator(SoAPointer soa, std::size_t index) : soa(soa), index(index) {}

    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Structure;
        using pointer           = Structure*;
        using reference         = std::conditional_t<std::is_same_v<SoAPointer, SoA*>, StructureRef, StructureConstRef>;

        Iterator(const Iterator& rhs) noexcept = default;
        Iterator& operator=(const Iterator& rhs) = default;

        reference operator*() const { return soa->at(index); }
        reference operator*() { return soa->at(index); }
        reference operator[](std::size_t n) { return *(*this + n); }

        // clang-format off
        Iterator& operator++() { ++index; return *this; }
        Iterator& operator--() { --index; return *this; }
        Iterator& operator+=(std::size_t n) { index += n; return *this; }
        Iterator& operator-=(std::size_t n) { index -= n; return *this; }
        Iterator operator+(std::size_t n) { return { soa, index + n }; }
        Iterator operator-(std::size_t n) { return { soa, index - n }; }
        difference_type operator-(const Iterator& rhs) const { return index - rhs.index; }
        bool operator!=(const Iterator& rhs) const { return index!= rhs.index; }
        auto operator<=>(const Iterator& rhs) const { return index<=> rhs.index; }

        const Iterator operator++(int) { Iterator it(*this); index++; return it; }
        const Iterator operator--(int) { Iterator it(*this); index--; return it; }
        // clang-format on
    };

    using iterator       = Iterator<SoA*>;
    using const_iterator = Iterator<const SoA*>;

    iterator       begin() noexcept { return {this, 0u}; }
    iterator       end() noexcept { return {this, m_Size}; }
    const_iterator begin() const noexcept { return {this, 0u}; }
    const_iterator end() const noexcept { return {this, m_Size}; }

    /* Capacity */

    [[nodiscard]] constexpr bool empty() const noexcept { return m_Size == 0; }
    constexpr std::size_t        size() const noexcept { return m_Size; }
    constexpr void               reserve(std::size_t n) {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (std::get<I>(m_Data).reserve(n), ...);
        }
        (std::index_sequence_for<Types...>{});
    }
    constexpr void shrink_to_fit() {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (std::get<I>(m_Data).shrink_to_fit(), ...);
        }
        (std::index_sequence_for<Types...>{});
    }

    /* Modifier */

    constexpr void clear() noexcept {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (std::get<I>(m_Data).clear(), ...);
        }
        (std::index_sequence_for<Types...>{});
        m_Size = 0;
    }
    constexpr void         push_back(StructureForward values);
    constexpr StructureRef emplace_back(Types&&... values);
    constexpr void         pop_back();
    constexpr void         resize(std::size_t count);
    constexpr void         resize(std::size_t count, StructureConstRef values);

private:
    void range_check(std::size_t i) const {
        if (i >= m_Size) {
            throw std::out_of_range(fmt::format(
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
constexpr void SoA<Types...>::push_back(StructureForward values) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).push_back(std::forward<TypeAt<I>>(std::get<I>(values))), ...);
    }
    (std::index_sequence_for<Types...>{});

    m_Size++;
}

template <typename... Types>
constexpr auto SoA<Types...>::emplace_back(Types&&... values) -> StructureRef {
    auto result = [&]<std::size_t... I>(std::index_sequence<I...>)->StructureRef {
        return std::tie(std::get<I>(m_Data).emplace_back(std::forward<Types>(values))...);
    }
    (std::index_sequence_for<Types...>{});

    m_Size++;

    return result;
}
template <typename... Types>
constexpr void SoA<Types...>::pop_back() {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).pop_back(), ...);
    }
    (std::index_sequence_for<Types...>{});

    m_Size--;
}
template <typename... Types>
constexpr void SoA<Types...>::resize(std::size_t count) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).resize(count), ...);
    }
    (std::index_sequence_for<Types...>{});

    m_Size = count;
}
template <typename... Types>
constexpr void SoA<Types...>::resize(std::size_t count, StructureConstRef values) {
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (std::get<I>(m_Data).resize(count, std::forward<const TypeAt<I>&>(std::get<I>(values))), ...);
    }
    (std::index_sequence_for<Types...>{});

    m_Size = count;
}

};  // namespace hitagi::utils