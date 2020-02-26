#include <algorithm>
#include "Buffer.hpp"

using namespace My;
Buffer::Buffer() : m_Data(nullptr), m_Size(0), m_Alignment(alignof(uint32_t)) {}
Buffer::Buffer(size_t size, const void* srcPtr, size_t copySize, size_t alignment)
    : m_Size(size), m_Alignment(alignment) {
    m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(size));
    if (srcPtr) memcpy(m_Data, srcPtr, std::min(size, copySize));
}
Buffer::Buffer(const Buffer& rhs) {
    m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(rhs.m_Size));
    memcpy(m_Data, rhs.m_Data, rhs.m_Size);
    m_Size      = rhs.m_Size;
    m_Alignment = rhs.m_Alignment;
}
Buffer::Buffer(Buffer&& rhs) {
    m_Data          = rhs.m_Data;
    m_Size          = rhs.m_Size;
    m_Alignment     = rhs.m_Alignment;
    rhs.m_Data      = nullptr;
    rhs.m_Size      = 0;
    rhs.m_Alignment = 4;
}
Buffer& Buffer::operator=(const Buffer& rhs) {
    if (m_Size >= rhs.m_Size && m_Alignment == rhs.m_Alignment)
        memcpy(m_Data, rhs.m_Data, rhs.m_Size);
    else {
        if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
        m_Data = reinterpret_cast<uint8_t*>(g_MemoryManager->Allocate(rhs.m_Size));
        memcpy(m_Data, rhs.m_Data, rhs.m_Size);
        m_Size      = rhs.m_Size;
        m_Alignment = rhs.m_Alignment;
    }
    return *this;
}
Buffer& Buffer::operator=(Buffer&& rhs) {
    if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
    m_Data          = rhs.m_Data;
    m_Size          = rhs.m_Size;
    m_Alignment     = rhs.m_Alignment;
    rhs.m_Data      = nullptr;
    rhs.m_Size      = 0;
    rhs.m_Alignment = 4;
    return *this;
}
Buffer::~Buffer() {
    if (m_Data) g_MemoryManager->Free(m_Data, m_Size);
    m_Data = nullptr;
}

uint8_t*       Buffer::GetData() { return m_Data; }
const uint8_t* Buffer::GetData() const { return m_Data; }
size_t         Buffer::GetDataSize() const { return m_Size; }
