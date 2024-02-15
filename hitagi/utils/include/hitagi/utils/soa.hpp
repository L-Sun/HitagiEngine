#pragma once
#include <hitagi/utils/concepts.hpp>

#include <fmt/format.h>
#include <range/v3/view/zip.hpp>

#include <cstddef>
#include <memory_resource>
#include <utility>

namespace hitagi::utils {

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
                return std::begin(ranges::views::zip(e...));
            },
            m_Data);
    }
    auto end() noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::end(ranges::views::zip(e...));
            },
            m_Data);
    }
    auto begin() const noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::begin(ranges::views::zip(e...));
            },
            m_Data);
    }
    auto end() const noexcept {
        return std::apply(
            [](auto&&... e) {
                return std::end(ranges::views::zip(e...));
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

};  // namespace hitagi::utils

template <typename... Types>
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