#include "Image.hpp"
#include "MemoryManager.hpp"

namespace My {
Image::Image(uint32_t width, uint32_t height, uint32_t bitcount, uint32_t pitch, size_t dataSize)
    : m_width(width), m_height(height), m_bitcount(bitcount), m_pitch(pitch), m_dataSize(dataSize) {
    m_pData = g_pMemoryManager->Allocate(dataSize);
}

Image::Image(const Image& rhs) {
    m_width    = rhs.m_width;
    m_height   = rhs.m_height;
    m_bitcount = rhs.m_bitcount;
    m_pitch    = rhs.m_pitch;
    m_dataSize = rhs.m_dataSize;
    m_pData    = g_pMemoryManager->Allocate(m_dataSize);
    std::memcpy(m_pData, rhs.m_pData, m_dataSize);
}
Image::Image(Image&& rhs) {
    m_width     = rhs.m_width;
    m_height    = rhs.m_height;
    m_bitcount  = rhs.m_bitcount;
    m_pitch     = rhs.m_pitch;
    m_dataSize  = rhs.m_dataSize;
    m_pData     = rhs.m_pData;
    rhs.m_pData = nullptr;
}
Image& Image::operator=(Image&& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, m_dataSize);
        m_width     = rhs.m_width;
        m_height    = rhs.m_height;
        m_bitcount  = rhs.m_bitcount;
        m_pitch     = rhs.m_pitch;
        m_dataSize  = rhs.m_dataSize;
        m_pData     = rhs.m_pData;
        rhs.m_pData = nullptr;
    }
    return *this;
}
Image& Image::operator=(const Image& rhs) {
    if (this != &rhs) {
        if (m_pData) g_pMemoryManager->Free(m_pData, m_dataSize);
        m_width    = rhs.m_width;
        m_height   = rhs.m_height;
        m_bitcount = rhs.m_bitcount;
        m_pitch    = rhs.m_pitch;
        m_dataSize = rhs.m_dataSize;
        m_pData    = g_pMemoryManager->Allocate(m_dataSize);
        std::memcpy(m_pData, rhs.m_pData, m_dataSize);
    }
    return *this;
}

Image::~Image() {
    if (m_pData) g_pMemoryManager->Free(m_pData, m_dataSize);
    m_pData = nullptr;
}

std::ostream& operator<<(std::ostream& out, const Image& image) {
    out << "Image" << std::endl;
    out << "-----" << std::endl;
    out << "Width: 0x" << image.m_width << std::endl;
    out << "Height: 0x" << image.m_height << std::endl;
    out << "Bit Count: 0x" << image.m_bitcount << std::endl;
    out << "Pitch: 0x" << image.m_pitch << std::endl;
    out << "Data Size: 0x" << image.m_dataSize << std::endl;
    return out;
}
}  // namespace My
