#include <hitagi/asset/parser/tga.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/math/vector.hpp>

#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::asset {

#pragma pack(push, 1)
struct TgaFileheader {
    std::uint8_t                 id_length;
    std::uint8_t                 color_map_type;
    std::uint8_t                 image_type;
    std::array<std::uint8_t, 5>  color_map_spec;
    std::array<std::uint8_t, 10> image_spec;
};
#pragma pack(pop)

std::shared_ptr<Texture> TgaParser::Parse(const core::Buffer& buffer) {
    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    if (buffer.Empty()) {
        logger->warn("[TGA] Parsing a empty buffer will return null");
        return nullptr;
    }

    auto data       = reinterpret_cast<const std::uint8_t*>(buffer.GetData());
    auto p_data_end = data + buffer.GetDataSize();

    logger->trace("[TGA] Parsing as TGA file:");
    const auto* file_header =
        reinterpret_cast<const TgaFileheader*>(data);
    data += sizeof(TgaFileheader);

    logger->trace("[TGA] ID Length:         {}", file_header->id_length);
    logger->trace("[TGA] Color Map Type:    {}", file_header->color_map_type);
    logger->trace("[TGA] Image Type:        {}", file_header->image_type);

    if (file_header->color_map_type) {
        logger->warn("[TGA] Unsupported Color Map. Only Type 0 is supported.");
        return nullptr;
    }

    if (file_header->image_type != 2) {
        logger->warn("[TGA] Unsupported Image Type. Only Type 2 is supported.");
        return nullptr;
    }

    auto width      = (file_header->image_spec[5] << 8) + file_header->image_spec[4];
    auto height     = (file_header->image_spec[7] << 8) + file_header->image_spec[6];
    auto pitch      = (width * 4 + 3) & ~3u;
    auto cpu_buffer = core::Buffer(pitch * height);

    std::uint8_t alpha_depth = file_header->image_spec[9] & 0x0F;
    int          pixel_depth = file_header->image_spec[8];
    logger->trace("[TGA] Image width:       {}", width);
    logger->trace("[TGA] Image height:      {}", height);
    logger->trace("[TGA] Image Pixel Depth: {}", pixel_depth);
    logger->trace("[TGA] Image Alpha Depth: {}", alpha_depth);
    // skip Image ID
    data += file_header->id_length;
    // skip the Color Map. since we assume the Color Map Type is 0,
    // nothing to skip

    auto out = cpu_buffer.Span<std::uint8_t>();
    for (auto i = 0; i < height; i++) {
        for (auto j = 0; j < width; j++) {
            switch (pixel_depth) {
                case 15: {
                    std::uint16_t color = *reinterpret_cast<const std::uint16_t*>(data);
                    data += 2;
                    out[pitch * i + j * 4]     = ((color & 0x7C00) >> 10);  // R
                    out[pitch * i + j * 4 + 1] = ((color & 0x03E0) >> 5);   // G
                    out[pitch * i + j * 4 + 2] = ((color & 0x001F) >> 10);  // B
                    out[pitch * i + j * 4 + 3] = 0xFF;                      // A
                } break;
                case 16: {
                    std::uint16_t color = *reinterpret_cast<const std::uint16_t*>(data);
                    data += 2;
                    out[pitch * i + j * 4]     = ((color & 0x7C00) >> 10);          // R
                    out[pitch * i + j * 4 + 1] = ((color & 0x03E0) >> 5);           // G
                    out[pitch * i + j * 4 + 2] = ((color & 0x001F) >> 10);          // B
                    out[pitch * i + j * 4 + 3] = ((color & 0x8000) ? 0xFF : 0x00);  // A
                } break;
                case 24: {
                    out[pitch * i + j * 4] = *data;  // R
                    data++;
                    out[pitch * i + j * 4 + 1] = *data;  // G
                    data++;
                    out[pitch * i + j * 4 + 2] = *data;  // B
                    data++;
                    out[pitch * i + j * 4 + 3] = 0xFF;  // A
                } break;
                case 32: {
                    out[pitch * i + j * 4] = *data;  // R
                    data++;
                    out[pitch * i + j * 4 + 1] = *data;  // G
                    data++;
                    out[pitch * i + j * 4 + 2] = *data;  // B
                    data++;
                    out[pitch * i + j * 4 + 3] = *data;  // A
                    data++;
                } break;
                default:
                    break;
            }
        }
    }
    assert(data <= p_data_end);

    return std::make_shared<Texture>(width, height, gfx::Format::R8G8B8A8_UNORM, std::move(cpu_buffer));
}
}  // namespace hitagi::asset