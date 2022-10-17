#include <hitagi/asset/parser/bmp.hpp>
#include <hitagi/math/vector.hpp>

#include <spdlog/logger.h>

using namespace hitagi::math;

namespace hitagi::asset {

#pragma pack(push, 1)
using BITMAP_FILEHEADER = struct BitmapFileheader {
    std::uint16_t signature;
    std::uint32_t size;
    std::uint32_t reserved;
    std::uint32_t bits_offset;
};
#define BITMAP_FILEHEADER_SIZE 14

using BITMAP_HEADER = struct BitmapHeader {
    std::uint32_t header_size;
    std::int32_t  width;
    std::int32_t  height;
    std::uint16_t planes;
    std::uint16_t bit_count;
    std::uint32_t compression;
    std::uint32_t size_image;
    std::int32_t  pels_per_meter_x;
    std::int32_t  pels_per_meter_y;
    std::uint32_t clr_used;
    std::uint32_t clr_important;
};
#pragma pack(pop)

std::shared_ptr<Texture> BmpParser::Parse(const core::Buffer& buffer) {
    if (buffer.Empty()) {
        m_Logger->warn("[BMP] Parsing a empty buffer will return nullptr");
        return nullptr;
    }

    auto file_header = reinterpret_cast<const BITMAP_FILEHEADER*>(buffer.GetData());
    auto bmp_header  = reinterpret_cast<const BITMAP_HEADER*>(buffer.GetData() + BITMAP_FILEHEADER_SIZE);
    if (file_header->signature == 0x4D42 /* 'B''M' */) {
        m_Logger->debug("[BMP] Asset is Windows BMP file");
        m_Logger->debug("[BMP] BMP Header");
        m_Logger->debug("[BMP] -----------------------------------");
        m_Logger->debug("[BMP] File Size:          {}", file_header->size);
        m_Logger->debug("[BMP] Data Offset:        {}", file_header->bits_offset);
        m_Logger->debug("[BMP] Image Width:        {}", bmp_header->width);
        m_Logger->debug("[BMP] Image Height:       {}", bmp_header->height);
        m_Logger->debug("[BMP] Image Planes:       {}", bmp_header->planes);
        m_Logger->debug("[BMP] Image BitCount:     {}", bmp_header->bit_count);
        m_Logger->debug("[BMP] Image Comperession: {}", bmp_header->compression);
        m_Logger->debug("[BMP] Image Size:         {}", bmp_header->size_image);

        if (bmp_header->bit_count < 24) {
            m_Logger->warn("[BMP] Sorry, only true color BMP is supported at now.");
            return nullptr;
        }

        auto width      = std::abs(bmp_header->width);
        auto height     = std::abs(bmp_header->height);
        auto pitch      = ((width * 4) + 3) & ~3;
        auto cpu_buffer = core::Buffer(pitch * height);

        auto           dest_data   = cpu_buffer.Span<math::R8G8B8A8Unorm>();
        const uint8_t* source_data = reinterpret_cast<const uint8_t*>(buffer.GetData()) + file_header->bits_offset;
        size_t         index       = 0;
        for (std::int32_t y = height - 1; y >= 0; y--) {
            for (std::uint32_t x = 0; x < width; x++) {
                dest_data[index].bgra = *reinterpret_cast<const R8G8B8A8Unorm*>(
                    source_data + pitch * y + x * 8);
                index++;
            }
        }

        return std::make_shared<Texture>(width, height, gfx::Format::R8G8B8A8_UNORM, std::move(cpu_buffer));
    }
    return nullptr;
}
}  // namespace hitagi::asset