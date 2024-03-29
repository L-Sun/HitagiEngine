#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <memory_resource>
#include <cassert>
#include <cstddef>

namespace hitagi::core {

class Buffer {
public:
    Buffer() = default;
    Buffer(std::size_t size, const std::byte* data = nullptr, std::size_t alignment = 4);
    Buffer(std::span<const std::byte> data, std::size_t alignment = 4);

    Buffer(const Buffer& buffer);
    Buffer(Buffer&& buffer) noexcept;

    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs) noexcept;

    ~Buffer();

    void Resize(std::size_t size, std::size_t alignment = 4);

    inline std::byte*       GetData() noexcept { return m_Data; }
    inline const std::byte* GetData() const noexcept { return m_Data; }
    inline auto             GetDataSize() const noexcept { return m_Size; }
    inline bool             Empty() const noexcept { return m_Data == nullptr || m_Size == 0; }

    template <typename T>
    std::span<const T> Span() const {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<const T>(reinterpret_cast<const T*>(m_Data), m_Size / sizeof(T));
    }

    template <typename T>
    std::span<T> Span() {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<T>(reinterpret_cast<T*>(m_Data), m_Size / sizeof(T));
    }

    auto Str() const noexcept {
        return std::string_view{reinterpret_cast<const char*>(m_Data), m_Size};
    }

private:
    std::pmr::polymorphic_allocator<> m_Allocator;
    std::byte*                        m_Data      = nullptr;
    std::size_t                       m_Size      = 0;
    std::size_t                       m_Alignment = alignof(uint32_t);
};
}  // namespace hitagi::core
