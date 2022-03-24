#pragma once
#include "Buffer.hpp"

#include <crossguid/guid.hpp>

#include <iostream>

namespace hitagi::asset {
class Image : public core::Buffer {
public:
    Image(uint32_t width, uint32_t height, uint32_t bitcount, uint32_t pitch, size_t data_size);
    Image() = default;

    inline xg::Guid GetGuid() const noexcept { return m_Guid; }
    inline uint32_t GetWidth() const noexcept { return m_Width; }
    inline uint32_t GetHeight() const noexcept { return m_Height; }
    inline uint32_t GetBitcount() const noexcept { return m_Bitcount; }
    inline uint32_t GetPitch() const noexcept { return m_Pitch; }

    friend std::ostream& operator<<(std::ostream& out, const Image& image);

private:
    xg::Guid m_Guid;
    uint32_t m_Width    = 0;
    uint32_t m_Height   = 0;
    uint32_t m_Bitcount = 0;
    uint32_t m_Pitch    = 0;
};

}  // namespace hitagi::asset
