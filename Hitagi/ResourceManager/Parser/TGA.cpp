#include "GeomMath.hpp"
#include "ImageParser.hpp"
#include "portable.hpp"
#include "TGA.hpp"

namespace Hitagi::Resource {

#pragma pack(push, 1)
struct TGA_FILEHEADER {
    uint8_t IDLength;
    uint8_t ColorMapType;
    uint8_t ImageType;
    uint8_t ColorMapSpec[5];
    uint8_t ImageSpec[10];
};
#pragma pack(pop)

Image TgaParser::Parse(const Core::Buffer& buf) {
    const uint8_t* data     = buf.GetData();
    const uint8_t* pDataEnd = data + buf.GetDataSize();

    m_Logger->debug("Parsing as TGA file:");
    const TGA_FILEHEADER* fileHeader =
        reinterpret_cast<const TGA_FILEHEADER*>(data);
    data += sizeof(TGA_FILEHEADER);

    m_Logger->debug("ID Length:      {}", fileHeader->IDLength);
    m_Logger->debug("Color Map Type: {}", fileHeader->ColorMapType);

    if (fileHeader->ColorMapType) {
        m_Logger->warn("Unsupported Color Map. Only Type 0 is supported.");
        return Image();
    }

    m_Logger->debug("Color Map Type: {}", fileHeader->ImageType);

    if (fileHeader->ImageType != 2) {
        m_Logger->warn("Unsupported Image Type. Only Type 2 is supported.");
        return Image();
    }
    // tga all values are little-endian
    auto    width      = (fileHeader->ImageSpec[5] << 8) + fileHeader->ImageSpec[4];
    auto    height     = (fileHeader->ImageSpec[7] << 8) + fileHeader->ImageSpec[6];
    uint8_t pixelDepth = fileHeader->ImageSpec[8];
    // rendering the pixel data
    auto bitcount = 32;
    // for GPU address alignment
    auto  pitch    = (width * (bitcount >> 3) + 3) & ~3u;
    auto  dataSize = pitch * height;
    Image img(width, height, bitcount, pitch, dataSize);

    uint8_t alpha_depth = fileHeader->ImageSpec[9] & 0x0F;
    m_Logger->debug("Image width:       {}", width);
    m_Logger->debug("Image height:      {}", height);
    m_Logger->debug("Image Pixel Depth: {}", pixelDepth);
    m_Logger->debug("Image Alpha Depth: {}", alpha_depth);
    // skip Image ID
    data += fileHeader->IDLength;
    // skip the Color Map. since we assume the Color Map Type is 0,
    // nothing to skip

    uint8_t* out = reinterpret_cast<uint8_t*>(img.getData());
    // clang-format off
        for (auto i = 0; i < height; i++) {
            for (auto j = 0; j < width; j++) {
                switch (pixelDepth) {
                    case 15:
                    {
                        uint16_t color = *reinterpret_cast<const uint16_t*>(data);
                        data += 2;
                        *(out + pitch * i + j * 4)     = ((color & 0x7C00) >> 10); // R
                        *(out + pitch * i + j * 4 + 1) = ((color & 0x03E0) >> 5);  // G
                        *(out + pitch * i + j * 4 + 2) = ((color & 0x001F) >> 10); // B
                        *(out + pitch * i + j * 4 + 3) = 0xFF;                     // A
                    } break;
                    case 16:
                    {
                        uint16_t color = *reinterpret_cast<const uint16_t*>(data);
                        data += 2;
                        *(out + pitch * i + j * 4)     = ((color & 0x7C00) >> 10);     // R
                        *(out + pitch * i + j * 4 + 1) = ((color & 0x03E0) >> 5);      // G
                        *(out + pitch * i + j * 4 + 2) = ((color & 0x001F) >> 10);     // B
                        *(out + pitch * i + j * 4 + 3) = ((color & 0x8000)?0xFF:0x00); // A
                    } break;
                    case 24:
                    {
                        *(out + pitch * i + j * 4)     = *data; // R
                        data++;
                        *(out + pitch * i + j * 4 + 1) = *data; // G
                        data++;
                        *(out + pitch * i + j * 4 + 2) = *data; // B
                        data++;
                        *(out + pitch * i + j * 4 + 3) = 0xFF;   // A
                    } break;
                    case 32:
                    {
                        *(out + pitch * i + j * 4)     = *data; // R
                        data++;
                        *(out + pitch * i + j * 4 + 1) = *data; // G
                        data++;
                        *(out + pitch * i + j * 4 + 2) = *data; // B
                        data++;
                        *(out + pitch * i + j * 4 + 3) = *data; // A
                        data++;
                    } break;
                    default:
                    break;
                }
            }
        }
        assert(data <= pDataEnd);
        return img;
}
}