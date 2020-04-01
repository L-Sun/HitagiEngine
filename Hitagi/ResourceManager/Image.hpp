#pragma once
#include <iostream>
#include "portable.hpp"

namespace Hitagi::Resource {
class Image {
public:
    Image() = default;
    Image(uint32_t width, uint32_t height, uint32_t bitcount, uint32_t pitch, size_t dataSize);

    Image(const Image& image);
    Image(Image&& image);
    Image& operator=(Image&& rhs);
    Image& operator=(const Image& rhs);
    ~Image();

    inline uint32_t GetWidth() const { return m_Width; }
    inline uint32_t GetHeight() const { return m_Height; }
    inline uint32_t GetBitcount() const { return m_Bitcount; }
    inline uint32_t GetPitch() const { return m_Pitch; }
    inline uint32_t GetDataSize() const { return m_DataSize; }
    inline void*    getData() const { return m_Data; }

    friend std::ostream& operator<<(std::ostream& out, const Image& image);

private:
    uint32_t m_Width    = 0;
    uint32_t m_Height   = 0;
    uint32_t m_Bitcount = 0;
    uint32_t m_Pitch    = 0;
    size_t   m_DataSize = 0;

    void* m_Data = nullptr;
};

}  // namespace Hitagi::Resource
