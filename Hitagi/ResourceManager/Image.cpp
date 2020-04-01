#include "Image.hpp"
#include "MemoryManager.hpp"

namespace Hitagi::Resource {

Image::Image(uint32_t width, uint32_t height, uint32_t bitcount, uint32_t pitch, size_t dataSize)
    : m_Width(width), m_Height(height), m_Bitcount(bitcount), m_Pitch(pitch), m_DataSize(dataSize) {
    m_Data = g_MemoryManager->Allocate(dataSize);
}

Image::Image(const Image& image)
    : m_Width(image.m_Width),
      m_Height(image.m_Height),
      m_Bitcount(image.m_Bitcount),
      m_Pitch(image.m_Pitch),
      m_DataSize(image.m_DataSize) {
    m_Data = g_MemoryManager->Allocate(m_DataSize);
    std::memcpy(m_Data, image.m_Data, m_DataSize);
}
Image::Image(Image&& image)
    : m_Width(image.m_Width),
      m_Height(image.m_Height),
      m_Bitcount(image.m_Bitcount),
      m_Pitch(image.m_Pitch),
      m_DataSize(image.m_DataSize),
      m_Data(image.m_Data) {
    image.m_Width    = 0;
    image.m_Height   = 0;
    image.m_Bitcount = 0;
    image.m_Pitch    = 0;
    image.m_DataSize = 0;
    image.m_Data     = nullptr;
}
Image& Image::operator=(Image&& rhs) {
    if (this != &rhs) {
        if (m_Data) g_MemoryManager->Free(m_Data, m_DataSize);
        m_Width    = rhs.m_Width;
        m_Height   = rhs.m_Height;
        m_Bitcount = rhs.m_Bitcount;
        m_Pitch    = rhs.m_Pitch;
        m_DataSize = rhs.m_DataSize;
        m_Data     = rhs.m_Data;

        rhs.m_Width    = 0;
        rhs.m_Height   = 0;
        rhs.m_Bitcount = 0;
        rhs.m_Pitch    = 0;
        rhs.m_DataSize = 0;
        rhs.m_Data     = nullptr;
    }
    return *this;
}
Image& Image::operator=(const Image& rhs) {
    if (this != &rhs) {
        if (m_Data) g_MemoryManager->Free(m_Data, m_DataSize);
        m_Width    = rhs.m_Width;
        m_Height   = rhs.m_Height;
        m_Bitcount = rhs.m_Bitcount;
        m_Pitch    = rhs.m_Pitch;
        m_DataSize = rhs.m_DataSize;
        m_Data     = g_MemoryManager->Allocate(m_DataSize);
        std::memcpy(m_Data, rhs.m_Data, m_DataSize);
    }
    return *this;
}

Image::~Image() {
    if (m_Data) g_MemoryManager->Free(m_Data, m_DataSize);
    m_Data = nullptr;
}

std::ostream& operator<<(std::ostream& out, const Image& image) {
    out << "Image" << std::endl;
    out << "-----" << std::endl;
    out << "Width: 0x" << image.m_Width << std::endl;
    out << "Height: 0x" << image.m_Height << std::endl;
    out << "Bit Count: 0x" << image.m_Bitcount << std::endl;
    out << "Pitch: 0x" << image.m_Pitch << std::endl;
    out << "Data Size: 0x" << image.m_DataSize << std::endl;
    return out;
}
}  // namespace Hitagi::Resource
