#include "TGA.hpp"

#include <spdlog/spdlog.h>

#include "HitagiMath.hpp"

namespace Hitagi::Asset {

#pragma pack(push, 1)
struct TgaFileheader {
    uint8_t                 id_length;
    uint8_t                 color_map_type;
    uint8_t                 image_type;
    std::array<uint8_t, 5>  color_map_spec;
    std::array<uint8_t, 10> image_spec;
};
#pragma pack(pop)

Image TgaParser::Parse(const Core::Buffer& buf) {
    auto logger = spdlog::get("AssetManager");
    if (buf.Empty()) {
        logger->warn("[TGA] Parsing a empty buffer will return empty image.");
        return Image{};
    }

    const uint8_t* data     = buf.GetData();
    const uint8_t* p_data_end = data + buf.GetDataSize();

    logger->debug("[TGA] Parsing as TGA file:");
    const auto* file_header =
        reinterpret_cast<const TgaFileheader*>(data);
    data += sizeof(TgaFileheader);

    logger->debug("[TGA] ID Length:         {}", file_header->id_length);
    logger->debug("[TGA] Color Map Type:    {}", file_header->color_map_type);
    logger->debug("[TGA] Image Type:        {}", file_header->image_type);

    if (file_header->color_map_type) {
        logger->warn("[TGA] Unsupported Color Map. Only Type 0 is supported.");
        return {};
    }

    if (file_header->image_type != 2) {
        logger->warn("[TGA] Unsupported Image Type. Only Type 2 is supported.");
        return {};
    }
    // tga all values are little-endian
    auto    width      = (file_header->image_spec[5] << 8) + file_header->image_spec[4];
    auto    height     = (file_header->image_spec[7] << 8) + file_header->image_spec[6];
    uint8_t pixel_depth = file_header->image_spec[8];
    // rendering the pixel data
    auto bitcount = 32;
    // for GPU address alignment
    auto  pitch    = (width * (bitcount >> 3) + 3) & ~3u;
    auto  data_size = pitch * height;
    Image img(width, height, bitcount, pitch, data_size);

    uint8_t alpha_depth = file_header->image_spec[9] & 0x0F;
    logger->debug("[TGA] Image width:       {}", width);
    logger->debug("[TGA] Image height:      {}", height);
    logger->debug("[TGA] Image Pixel Depth: {}", pixel_depth);
    logger->debug("[TGA] Image Alpha Depth: {}", alpha_depth);
    // skip Image ID
    data += file_header->id_length;
    // skip the Color Map. since we assume the Color Map Type is 0,
    // nothing to skip

    auto* out = reinterpret_cast<uint8_t*>(img.GetData());
    // clang-format off
        for (auto i = 0; i < height; i++) {
            for (auto j = 0; j < width; j++) {
                switch (pixel_depth) {
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
        assert(data <= p_data_end);
        return img;
}
}  // namespace Hitagi::Resource