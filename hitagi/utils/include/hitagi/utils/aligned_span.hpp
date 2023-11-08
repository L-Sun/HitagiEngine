#pragma once
#include <hitagi/utils/utils.hpp>

#include <iterator>

namespace hitagi::utils {

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

}  // namespace hitagi::utils