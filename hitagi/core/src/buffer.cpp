#include <hitagi/core/buffer.hpp>

#include <algorithm>
#include <memory_resource>
#include <type_traits>

namespace hitagi::core {

Buffer::Buffer(size_t size, size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment))),
      m_Size(size),
      m_Alignment(alignment) {
}

Buffer::Buffer(const void* initial_data, size_t size, size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment))),
      m_Size(size),
      m_Alignment(alignment) {
    std::memcpy(m_Data, initial_data, m_Size);
}

Buffer::Buffer(const Buffer& other)
    : m_Allocator(other.m_Allocator),
      m_Data(static_cast<std::byte*>(m_Allocator.allocate_bytes(other.m_Size, other.m_Alignment))),
      m_Size(other.m_Size),
      m_Alignment(other.m_Alignment) {
    std::memcpy(m_Data, other.m_Data, other.m_Size);
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_Allocator(other.m_Allocator),
      m_Data(other.m_Data),
      m_Size(other.m_Size),
      m_Alignment(other.m_Alignment) {
    other.m_Data = nullptr;
    other.m_Size = 0;
}

Buffer& Buffer::operator=(const Buffer& rhs) {
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
}

Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
    if (this != &rhs) {
        if (m_Allocator != rhs.m_Allocator && m_Data != nullptr) {
            m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
            m_Data = static_cast<std::byte*>(m_Allocator.allocate_bytes(m_Size, m_Alignment));
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        } else {
            m_Data = rhs.m_Data;
        }

        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;

        rhs.m_Data      = nullptr;
        rhs.m_Size      = 0;
        rhs.m_Alignment = 0;
    }
    return *this;
}
Buffer::~Buffer() {
    Clear();
}

void Buffer::Resize(std::size_t size, std::size_t alignment) {
    auto data = static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment));
    if (m_Data) {
        std::memcpy(data, m_Data, std::min(size, m_Size));
        m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
    }
    m_Data      = data;
    m_Size      = size;
    m_Alignment = alignment;
}

void Buffer::Clear() {
    if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
    m_Data      = nullptr;
    m_Size      = 0;
    m_Alignment = 0;
}

}  // namespace hitagi::core