#include <hitagi/core/buffer.hpp>

#include <algorithm>
#include <memory_resource>
#include <type_traits>

namespace hitagi::core {

Buffer::Buffer(size_t size, size_t alignment)
    : m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment))),
      m_Size(size),
      m_Alignment(alignment) {
}

Buffer::Buffer(const void* initial_data, size_t size, size_t alignment)
    : m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment))),
      m_Size(size),
      m_Alignment(alignment) {
    std::memcpy(m_Data, initial_data, m_Size);
}

Buffer::Buffer(const Buffer& buffer)
    : m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(buffer.m_Size, buffer.m_Alignment))),
      m_Size(buffer.m_Size),
      m_Alignment(buffer.m_Alignment) {
    std::memcpy(m_Data, buffer.m_Data, buffer.m_Size);
}

Buffer::Buffer(Buffer&& buffer) noexcept
    : m_Allocator(buffer.m_Allocator),
      m_Data(buffer.m_Data),
      m_Size(buffer.m_Size),
      m_Alignment(buffer.m_Alignment) {
    buffer.m_Data      = nullptr;
    buffer.m_Size      = 0;
    buffer.m_Alignment = 0;
}

auto Buffer::operator=(const Buffer& rhs) -> Buffer& {
    if (this != &rhs) {
        if (m_Size >= rhs.m_Size && m_Alignment == rhs.m_Alignment)
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        else {
            if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
            m_Data = static_cast<std::byte*>(m_Allocator.allocate_bytes(rhs.m_Size, rhs.m_Alignment));
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
            m_Size      = rhs.m_Size;
            m_Alignment = rhs.m_Alignment;
        }
    }
    return *this;
}  // namespace hitagi::Core
auto Buffer::operator=(Buffer&& rhs) noexcept -> Buffer& {
    if (this != &rhs) {
        if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);

        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;

        if (m_Allocator == rhs.m_Allocator) {
            m_Data          = rhs.m_Data;
            rhs.m_Data      = nullptr;
            rhs.m_Size      = 0;
            rhs.m_Alignment = 0;
        } else {
            m_Data = static_cast<std::byte*>(m_Allocator.allocate_bytes(m_Size, m_Alignment));
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        }
    }
    return *this;
}
Buffer::~Buffer() {
    if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
    m_Data = nullptr;
}

}  // namespace hitagi::core