#include "Buffer.hpp"

#include <algorithm>

#include "MemoryManager.hpp"

namespace Hitagi::Core {

Buffer::Buffer(size_t size, size_t alignment)
    : m_Size(size),
      m_Alignment(alignment) {
    m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(size, m_Alignment));
}

Buffer::Buffer(const void* initialData, size_t size, size_t alignment)
    : m_Size(size),
      m_Alignment(alignment) {
    m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(size, m_Alignment));
    std::memcpy(m_Data, initialData, m_Size);
}

Buffer::Buffer(const Buffer& buffer)
    : m_Data(reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(buffer.m_Size, buffer.m_Alignment))),
      m_Size(buffer.m_Size),
      m_Alignment(buffer.m_Alignment) {
    std::memcpy(m_Data, buffer.m_Data, buffer.m_Size);
}
Buffer::Buffer(Buffer&& buffer) : m_Data(buffer.m_Data), m_Size(buffer.m_Size), m_Alignment(buffer.m_Alignment) {
    buffer.m_Data      = nullptr;
    buffer.m_Size      = 0;
    buffer.m_Alignment = 0;
}
auto Buffer::operator=(const Buffer& rhs) -> Buffer& {
    if (this != &rhs) {
        if (m_Size >= rhs.m_Size && m_Alignment == rhs.m_Alignment)
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        else {
            if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
            m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(rhs.m_Size, rhs.m_Alignment));
            std::memcpy(m_Data, rhs.m_Data, rhs.m_Size);
            m_Size      = rhs.m_Size;
            m_Alignment = rhs.m_Alignment;
        }
    }
    return *this;
}  // namespace Hitagi::Core
auto Buffer::operator=(Buffer&& rhs) -> Buffer& {
    if (this != &rhs) {
        if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
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
    if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
    m_Data = nullptr;
}

auto Buffer::GetData() -> uint8_t* {
    return m_Data;
}
auto Buffer::GetData() const -> const uint8_t* {
    return m_Data;
}
auto Buffer::GetDataSize() const -> size_t {
    return m_Size;
}
}  // namespace Hitagi::Core