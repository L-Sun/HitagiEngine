#include "TGA.hpp"

#include <spdlog/spdlog.h>

#include "HitagiMath.hpp"

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

    spdlog::get("ResourceManager")->debug("[TGA] Parsing as TGA file:");
    const TGA_FILEHEADER* fileHeader =
        reinterpret_cast<const TGA_FILEHEADER*>(data);
    data += sizeof(TGA_FILEHEADER);

    spdlog::get("ResourceManager")->debug("[TGA] ID Length:         {}", fileHeader->IDLength);
    spdlog::get("ResourceManager")->debug("[TGA] Color Map Type:    {}", fileHeader->ColorMapType);
    spdlog::get("ResourceManager")->debug("[TGA] Image Type:        {}", fileHeader->ImageType);

    if (fileHeader->ColorMapType) {
        spdlog::get("ResourceManager")->warn("[TGA] Unsupported Color Map. Only Type 0 is supported.");
        return Image();
    }

    if (fileHeader->ImageType != 2) {
        spdlog::get("ResourceManager")->warn("[TGA] Unsupported Image Type. Only Type 2 is supported.");
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
    spdlog::get("ResourceManager")->debug("[TGA] Image width:       {}", width);
    spdlog::get("ResourceManager")->debug("[TGA] Image height:      {}", height);
    spdlog::get("ResourceManager")->debug("[TGA] Image Pixel Depth: {}", pixelDepth);
    spdlog::get("ResourceManager")->debug("[TGA] Image Alpha Depth: {}", alpha_depth);
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
}  // namespace Hitagi::Resource