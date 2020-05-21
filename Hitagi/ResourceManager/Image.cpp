#include "Image.hpp"

#include <fmt/format.h>
#include <array>

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
    double               size = image.m_DataSize;
    constexpr std::array unit = {"B", "kB", "MB"};
    size_t               i    = 0;
    for (; i < 2; i++) {
        if (size < 10) break;
        size /= 1024;
    }

    return out << fmt::format(
               "{0:-^25}\n"
               "Width     {1:>10} px\n"
               "Height    {2:>10} px\n"
               "Bit Count {3:>10} bits\n"
               "Pitch     {4:>10} \n"
               "Data Size {5:>10.2f} {6}\n"
               "{7:-^25}",
               "Image Info", image.m_Width, image.m_Height, image.m_Bitcount, image.m_Pitch, size, unit[i], "End");
}
}  // namespace Hitagi::Resource
