export module utils:strided_span;
import std;

export namespace hitagi::utils {

template <typename T>
class StridedSpan {
public:
    StridedSpan(std::byte* data, std::size_t element_count, std::size_t element_stride)
        : m_Data(data), m_ElementCount(element_count), m_ElementStride(element_stride) {}

    // A T* supports contiguous pointer arithmetic only.
    inline auto data() const -> T* {
        if (m_ElementCount > 1 && m_ElementStride != sizeof(T)) {
            throw std::logic_error("StridedSpan::data requires contiguous elements");
        }
        return reinterpret_cast<T*>(m_Data);
    }
    inline auto size() const noexcept -> std::size_t { return m_ElementCount; }
    inline auto element_stride() const noexcept -> std::size_t { return m_ElementStride; }
    inline auto element_size() const noexcept -> std::size_t { return sizeof(T); }

    inline auto front() noexcept -> T& { return *reinterpret_cast<T*>(m_Data); }

    inline auto back() noexcept -> T& { return *reinterpret_cast<T*>(m_Data + (m_ElementCount - 1) * element_stride()); }

    inline auto operator[](std::size_t index) noexcept -> T& { return *reinterpret_cast<T*>(m_Data + index * element_stride()); }

    class Iterator {
    public:
        // clang-format off
        // std::input_iterator
        using value_type = std::remove_cv_t<T>;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        inline auto operator*() const -> reference { return (*strided_span)[index]; }
        inline auto operator-(const Iterator& rhs) const noexcept -> difference_type { return static_cast<difference_type>(index) - static_cast<difference_type>(rhs.index); }
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
        inline auto operator+(difference_type n) const noexcept -> Iterator { return { strided_span, index + n }; }
        inline auto operator-=(difference_type n) noexcept -> Iterator& { index -= n; return *this; }
        inline auto operator-(difference_type n) const noexcept -> Iterator { return { strided_span, index - n }; }
        inline auto operator[](std::size_t n) const noexcept -> reference { return *(*this + n); }
        inline friend auto operator+(difference_type n, const Iterator& rhs) noexcept -> Iterator { return rhs + n; }

        // clang-format on
    private:
        friend StridedSpan;
        Iterator(StridedSpan* strided_span, std::size_t index) : strided_span(strided_span), index(index) {}
        StridedSpan* strided_span;
        std::size_t  index;
    };
    static_assert(std::random_access_iterator<Iterator>);

    auto begin() noexcept -> Iterator { return {this, 0}; }
    auto end() noexcept -> Iterator { return {this, m_ElementCount}; }

protected:
    std::byte*  m_Data;
    std::size_t m_ElementCount;
    std::size_t m_ElementStride;
};

}  // namespace hitagi::utils
