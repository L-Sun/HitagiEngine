#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <memory_resource>
#include <cassert>

namespace hitagi::core {

class Buffer {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    Buffer(allocator_type alloc = {});
    Buffer(std::size_t size, std::size_t alignment = 4, allocator_type alloc = {});
    Buffer(const void* initial_data, std::size_t size, std::size_t alignment = 4, allocator_type alloc = {});

    Buffer(const Buffer& buffer, allocator_type alloc = {});
    Buffer(Buffer&& buffer) noexcept = default;

    Buffer& operator=(const Buffer& rhs);
    Buffer& operator=(Buffer&& rhs) noexcept;

    ~Buffer();

    void Resize(std::size_t size, std::size_t alignment = 4);
    void Clear();

    inline std::byte*       GetData() noexcept { return m_Data; }
    inline const std::byte* GetData() const noexcept { return m_Data; }
    inline auto             GetDataSize() const noexcept { return m_Size; }
    inline bool             Empty() const noexcept { return m_Data == nullptr || m_Size == 0; }

    template <typename T>
    std::span<T> Span() const {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<T>{m_Data, m_Size / sizeof(T)};
    }

    template <typename T>
    std::span<T> Span() {
        assert(
            m_Size % sizeof(T) == 0 &&
            "Create span from buffer failed,"
            " since the buffer size is not multiple of sizeof(T)");
        return std::span<T>(reinterpret_cast<T*>(m_Data), m_Size / sizeof(T));
    }

private:
    allocator_type m_Allocator;
    std::byte*     m_Data      = nullptr;
    std::size_t    m_Size      = 0;
    std::size_t    m_Alignment = alignof(uint32_t);
};
}  // namespace hitagi::core
