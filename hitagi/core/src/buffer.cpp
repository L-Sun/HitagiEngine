#include <hitagi/core/buffer.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <algorithm>

namespace hitagi::core {

Buffer::Buffer(size_t size, size_t alignment)
    : m_Size(size),
      m_Alignment(alignment) {
    m_Data = g_MemoryManager->Allocate<std::byte>(size, m_Alignment);
}

Buffer::Buffer(const void* initial_data, size_t size, size_t alignment)
    : m_Size(size),
      m_Alignment(alignment) {
    m_Data = g_MemoryManager->Allocate<std::byte>(size, m_Alignment);
    std::memcpy(m_Data, initial_data, m_Size);
}

Buffer::Buffer(const Buffer& buffer)
    : m_Data(g_MemoryManager->Allocate<std::byte>(buffer.m_Size, buffer.m_Alignment)),
      m_Size(buffer.m_Size),
      m_Alignment(buffer.m_Alignment) {
    std::memcpy(m_Data, buffer.m_Data, buffer.m_Size);
}

Buffer::Buffer(Buffer&& buffer) noexcept : m_Data(buffer.m_Data), m_Size(buffer.m_Size), m_Alignment(buffer.m_Alignment) {
    buffer.m_Data      = nullptr;
    buffer.m_Size      = 0;
    buffer.m_Alignment = 0;
}

auto Buffer::operator=(const Buffer& rhs) -> Buffer& {
    if (this != &rhs) {
        if (m_Size >= rhs.m_Size && m_Alignment == rhs.m_Alignment)
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        else {
            if (m_Data) g_MemoryManager->Free(m_Data, m_Size, m_Alignment);
            m_Data = g_MemoryManager->Allocate<std::byte>(rhs.m_Size, rhs.m_Alignment);
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
            m_Size      = rhs.m_Size;
            m_Alignment = rhs.m_Alignment;
        }
    }
    return *this;
}  // namespace hitagi::Core
auto Buffer::operator=(Buffer&& rhs) noexcept -> Buffer& {
    if (this != &rhs) {
        if (m_Data) g_MemoryManager->Free(m_Data, m_Size, m_Alignment);
        m_Data          = rhs.m_Data;
        m_Size          = rhs.m_Size;
        m_Alignment     = rhs.m_Alignment;
        rhs.m_Data      = nullptr;
        rhs.m_Size      = 0;
        rhs.m_Alignment = 0;
    }
    return *this;
}
Buffer::~Buffer() {
    if (m_Data) g_MemoryManager->Free(m_Data, m_Size, m_Alignment);
    m_Data = nullptr;
}

}  // namespace hitagi::core