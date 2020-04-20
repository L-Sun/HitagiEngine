#include <iostream>
#include "GeomMath.hpp"
#include "BMP.hpp"

namespace Hitagi::Resource {

Image BmpParser::Parse(const Core::Buffer& buf) {
    Image                    img;
    const BITMAP_FILEHEADER* fileHeader =
        reinterpret_cast<const BITMAP_FILEHEADER*>(buf.GetData());
    const BITMAP_HEADER* bmpHeader = reinterpret_cast<const BITMAP_HEADER*>(
        buf.GetData() + BITMAP_FILEHEADER_SIZE);
    if (fileHeader->Signature == 0x4D42 /* 'B''M' */) {
        m_Logger->debug("Asset is Windows BMP file");
        m_Logger->debug("BMP Header");
        m_Logger->debug("-----------------------------------");
        m_Logger->debug("File Size:          {}", fileHeader->Size);
        m_Logger->debug("Data Offset:        {}", fileHeader->BitsOffset);
        m_Logger->debug("Image Width:        {}", bmpHeader->Width);
        m_Logger->debug("Image Height:       {}", bmpHeader->Height);
        m_Logger->debug("Image Planes:       {}", bmpHeader->Planes);
        m_Logger->debug("Image BitCount:     {}", bmpHeader->BitCount);
        m_Logger->debug("Image Comperession: {}", bmpHeader->Compression);
        m_Logger->debug("Image Size:         {}", bmpHeader->SizeImage);

        auto     width      = bmpHeader->Width;
        auto     height     = bmpHeader->Height;
        auto     bitcount   = 32;
        auto     byte_count = bitcount >> 3;
        auto     pitch      = ((width * bitcount >> 3) + 3) & ~3;
        auto     dataSize   = pitch * height;
        Image    img(width, height, bitcount, pitch, dataSize);
        uint8_t* data = reinterpret_cast<uint8_t*>(img.getData());
        if (bitcount < 24) {
            m_Logger->warn("Sorry, only true color BMP is supported at now.");
        } else {
            const uint8_t* sourceData =
                reinterpret_cast<const uint8_t*>(buf.GetData()) +
                fileHeader->BitsOffset;
            for (int32_t y = height - 1; y >= 0; y--) {
                for (uint32_t x = 0; x < width; x++) {
                    (reinterpret_cast<R8G8B8A8Unorm*>(data +
                                                      pitch * (height - y - 1) + x * byte_count))
                        ->bgra = *reinterpret_cast<const R8G8B8A8Unorm*>(
                        sourceData + pitch * y + x * byte_count);
                }
            }
        }
    }
    return img;
}
}  // namespace Hitagi::Resource