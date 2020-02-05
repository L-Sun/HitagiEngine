#include "geommath.hpp"
#include "ImageParser.hpp"
#include "portable.hpp"
#include "TGA.hpp"

using namespace My;

#pragma pack(push, 1)
struct TGA_FILEHEADER {
    uint8_t IDLength;
    uint8_t ColorMapType;
    uint8_t ImageType;
    uint8_t ColorMapSpec[5];
    uint8_t ImageSpec[10];
};
#pragma pack(pop)

Image TgaParser::Parse(const Buffer& buf) {
    const uint8_t* pData    = buf.GetData();
    const uint8_t* pDataEnd = pData + buf.GetDataSize();

    std::cout << "Parsing as TGA file:" << std::endl;

    const TGA_FILEHEADER* pFileHeader =
        reinterpret_cast<const TGA_FILEHEADER*>(pData);
    pData += sizeof(TGA_FILEHEADER);

#ifdef DEBUG
    std::cout << "ID Length: " << (uint16_t)pFileHeader->IDLength << std::endl;
    std::cout << "Color Map Type: " << (uint16_t)pFileHeader->ColorMapType
              << std::endl;
#endif  // DEBUG

    if (pFileHeader->ColorMapType) {
        std::cout << "Unsupported Color Map. Only Type 0 is supported."
                  << std::endl;
        return Image();
    }

#ifdef DEBUG
    std::cout << "Image Type: " << (uint16_t)pFileHeader->ImageType
              << std::endl;
#endif

    if (pFileHeader->ImageType != 2) {
        std::cout << "Unsupported Image Type. Only Type 2 is supported."
                  << std::endl;
    }
    // tga all values are little-endian
    auto    width       = (pFileHeader->ImageSpec[5] << 8) + pFileHeader->ImageSpec[4];
    auto    height      = (pFileHeader->ImageSpec[7] << 8) + pFileHeader->ImageSpec[6];
    uint8_t pixel_depth = pFileHeader->ImageSpec[8];
    // rendering the pixel data
    auto bitcount = 32;
    // for GPU address alignment
    auto  pitch     = (width * (bitcount >> 3) + 3) & ~3u;
    auto  data_size = pitch * height;
    Image img(width, height, bitcount, pitch, data_size);

#ifdef DEBUG
    uint8_t alpha_depth = pFileHeader->ImageSpec[9] & 0x0F;
    std::cout << "Image width: " << width << std::endl;
    std::cout << "Image height: " << height << std::endl;
    std::cout << "Image Pixel Depth: " << (uint16_t)pixel_depth << std::endl;
    std::cout << "Image Alpha Depth: " << (uint16_t)alpha_depth << std::endl;
#endif
    // skip Image ID
    pData += pFileHeader->IDLength;
    // skip the Color Map. since we assume the Color Map Type is 0,
    // nothing to skip

    uint8_t* pOut = reinterpret_cast<uint8_t*>(img.getData());
    // clang-format off
        for (auto i = 0; i < height; i++) {
            for (auto j = 0; j < width; j++) {
                switch (pixel_depth) {
                    case 15:
                    {
                        uint16_t color = *reinterpret_cast<const uint16_t*>(pData);
                        pData += 2;
                        *(pOut + pitch * i + j * 4)     = ((color & 0x7C00) >> 10); // R
                        *(pOut + pitch * i + j * 4 + 1) = ((color & 0x03E0) >> 5);  // G
                        *(pOut + pitch * i + j * 4 + 2) = ((color & 0x001F) >> 10); // B
                        *(pOut + pitch * i + j * 4 + 3) = 0xFF;                     // A
                    } break;
                    case 16:
                    {
                        uint16_t color = *reinterpret_cast<const uint16_t*>(pData);
                        pData += 2;
                        *(pOut + pitch * i + j * 4)     = ((color & 0x7C00) >> 10);     // R
                        *(pOut + pitch * i + j * 4 + 1) = ((color & 0x03E0) >> 5);      // G
                        *(pOut + pitch * i + j * 4 + 2) = ((color & 0x001F) >> 10);     // B
                        *(pOut + pitch * i + j * 4 + 3) = ((color & 0x8000)?0xFF:0x00); // A
                    } break;
                    case 24:
                    {
                        *(pOut + pitch * i + j * 4)     = *pData; // R
                        pData++;
                        *(pOut + pitch * i + j * 4 + 1) = *pData; // G
                        pData++;
                        *(pOut + pitch * i + j * 4 + 2) = *pData; // B
                        pData++;
                        *(pOut + pitch * i + j * 4 + 3) = 0xFF;   // A
                    } break;
                    case 32:
                    {
                        *(pOut + pitch * i + j * 4)     = *pData; // R
                        pData++;
                        *(pOut + pitch * i + j * 4 + 1) = *pData; // G
                        pData++;
                        *(pOut + pitch * i + j * 4 + 2) = *pData; // B
                        pData++;
                        *(pOut + pitch * i + j * 4 + 3) = *pData; // A
                        pData++;
                    } break;
                    default:
                    break;
                }
            }
        }
        assert(pData <= pDataEnd);
        return img;
    }