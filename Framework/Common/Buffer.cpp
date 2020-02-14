#include <algorithm>
#include "Buffer.hpp"

using namespace My;
Buffer::Buffer() : m_pData(nullptr), m_szSize(0), m_szAlignment(alignof(uint32_t)) {}
Buffer::Buffer(size_t size, const void* srcPtr, size_t copySize, size_t alignment)
    : m_szSize(size), m_szAlignment(alignment) {
    m_pData = reinterpret_cast<uint8_t*>(g_pMemoryManager->Allocate(size));
    if (srcPtr) memcpy(m_pData, srcPtr, std::min(size, copySize));
}
Buffer::Buffer(const Buffer& rhs) {
    m_pData = reinterpret_cast<uint8_t*>(g_pMemoryManager->Allocate(rhs.m_szSize));
    memcpy(m_pData, rhs.m_pData, rhs.m_szSize);
    m_szSize      = rhs.m_szSize;
    m_szAlignment = rhs.m_szAlignment;
}
Buffer::Buffer(Buffer&& rhs) {
    m_pData           = rhs.m_pData;
    m_szSize          = rhs.m_szSize;
    m_szAlignment     = rhs.m_szAlignment;
    rhs.m_pData       = nullptr;
    rhs.m_szSize      = 0;
    rhs.m_szAlignment = 4;
}
Buffer& Buffer::operator=(const Buffer& rhs) {
    if (m_szSize >= rhs.m_szSize && m_szAlignment == rhs.m_szAlignment)
        memcpy(m_pData, rhs.m_pData, rhs.m_szSize);
    else {
        if (m_pData) g_pMemoryManager->Free(m_pData, m_szSize);
        m_pData = reinterpret_cast<uint8_t*>(g_pMemoryManager->Allocate(rhs.m_szSize));
        memcpy(m_pData, rhs.m_pData, rhs.m_szSize);
        m_szSize      = rhs.m_szSize;
        m_szAlignment = rhs.m_szAlignment;
    }
    return *this;
}
Buffer& Buffer::operator=(Buffer&& rhs) {
    if (m_pData) g_pMemoryManager->Free(m_pData, m_szSize);
    m_pData           = rhs.m_pData;
    m_szSize          = rhs.m_szSize;
    m_szAlignment     = rhs.m_szAlignment;
    rhs.m_pData       = nullptr;
    rhs.m_szSize      = 0;
    rhs.m_szAlignment = 4;
    return *this;
}
Buffer::~Buffer() {
    if (m_pData) g_pMemoryManager->Free(m_pData, m_szSize);
    m_pData = nullptr;
}

uint8_t*       Buffer::GetData() { return m_pData; }
const uint8_t* Buffer::GetData() const { return m_pData; }
size_t         Buffer::GetDataSize() const { return m_szSize; }
