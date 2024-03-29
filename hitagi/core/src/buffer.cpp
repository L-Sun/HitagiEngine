#include <hitagi/core/buffer.hpp>

#include <algorithm>
#include <memory_resource>
#include <cstring>

namespace hitagi::core {

Buffer::Buffer(size_t size, const std::byte* data, size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(size != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(size, alignment)) : nullptr),
      m_Size(size),
      m_Alignment(alignment)

{
    if (data != nullptr) {
        std::memcpy(m_Data, data, m_Size);
    }
}

Buffer::Buffer(std::span<const std::byte> data, std::size_t alignment)
    : m_Allocator(std::pmr::get_default_resource()),
      m_Data(data.size() != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(data.size(), alignment)) : nullptr),
      m_Size(data.size()),
      m_Alignment(alignment)

{
    if (m_Size != 0) std::memcpy(m_Data, data.data(), m_Size);
}

Buffer::Buffer(const Buffer& other)
    : m_Allocator(other.m_Allocator),
      m_Data(other.m_Size != 0 ? static_cast<std::byte*>(m_Allocator.allocate_bytes(other.m_Size, other.m_Alignment)) : nullptr),
      m_Size(other.m_Size),
      m_Alignment(other.m_Alignment) {
    if (m_Size == 0) return;
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
        if (m_Size >= rhs.m_Size && m_Alignment == rhs.m_Alignment) {
            if (rhs.m_Size != 0) {
                std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
            }
        } else {
            if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);
            if (rhs.m_Size != 0) {
                m_Data = static_cast<std::byte*>(m_Allocator.allocate_bytes(rhs.m_Size, rhs.m_Alignment));
                std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
            }
        }
        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;
    }
    return *this;
}

Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
    if (this != &rhs) {
        // use copy
        if (m_Allocator != rhs.m_Allocator) {
            return this->operator=(std::cref(rhs));
        } else {
            if (m_Data != nullptr) m_Allocator.deallocate(m_Data, m_Size);
        }
        m_Data      = rhs.m_Data;
        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;

        rhs.m_Data      = nullptr;
        rhs.m_Size      = 0;
        rhs.m_Alignment = 0;
    }
    return *this;
}
Buffer::~Buffer() {
    if (m_Data) m_Allocator.deallocate_bytes(m_Data, m_Size, m_Alignment);

    m_Data      = nullptr;
    m_Size      = 0;
    m_Alignment = 0;
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

}  // namespace hitagi::core