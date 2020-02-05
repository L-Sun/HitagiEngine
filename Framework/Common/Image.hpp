#pragma once
#include <iostream>

namespace My {
class Image {
public:
    Image() = default;
    Image(uint32_t m_width, uint32_t m_height, uint32_t m_bitcount,
          uint32_t m_pitch, size_t m_dataSize);

    Image(const Image& rhs);
    Image(Image&& rhs);
    Image operator=(Image&& rhs);
    Image operator=(const Image& rhs);
    ~Image();

    inline uint32_t GetWidth() const { return m_width; }
    inline uint32_t GetHeight() const { return m_height; }
    inline uint32_t GetBitcount() const { return m_bitcount; }
    inline uint32_t GetPitch() const { return m_pitch; }
    inline uint32_t GetDataSize() const { return m_dataSize; }
    inline void*    getData() const { return m_pData; }

    friend std::ostream& operator<<(std::ostream& out, const Image& image);

private:
    uint32_t m_width    = 0;
    uint32_t m_height   = 0;
    uint32_t m_bitcount = 0;
    uint32_t m_pitch    = 0;
    size_t   m_dataSize = 0;

    void* m_pData = nullptr;
};

}  // namespace My
