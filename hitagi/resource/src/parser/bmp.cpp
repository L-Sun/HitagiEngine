#include <hitagi/parser/bmp.hpp>
#include <hitagi/math/vector.hpp>

#include <spdlog/spdlog.h>

using namespace hitagi::math;

namespace hitagi::resource {

#pragma pack(push, 1)
using BITMAP_FILEHEADER = struct BitmapFileheader {
    uint16_t signature;

    uint32_t size;

    uint32_t reserved;

    uint32_t bits_offset;
};
#define BITMAP_FILEHEADER_SIZE 14

using BITMAP_HEADER = struct BitmapHeader {
    uint32_t header_size;

    int32_t width;

    int32_t height;

    uint16_t planes;

    uint16_t bit_count;

    uint32_t compression;

    uint32_t size_image;

    int32_t pels_per_meter_x;

    int32_t pels_per_meter_y;

    uint32_t clr_used;

    uint32_t clr_important;
};
#pragma pack(pop)

std::shared_ptr<Image> BmpParser::Parse(const core::Buffer& buf, allocator_type alloc) {
    auto logger = spdlog::get("AssetManager");
    if (buf.Empty()) {
        logger->warn("[BMP] Parsing a empty buffer will return nullptr");
        return nullptr;
    }

    const auto* file_header =
        reinterpret_cast<const BITMAP_FILEHEADER*>(buf.GetData());
    const auto* bmp_header = reinterpret_cast<const BITMAP_HEADER*>(
        buf.GetData() + BITMAP_FILEHEADER_SIZE);
    if (file_header->signature == 0x4D42 /* 'B''M' */) {
        logger->debug("[BMP] Asset is Windows BMP file");
        logger->debug("[BMP] BMP Header");
        logger->debug("[BMP] -----------------------------------");
        logger->debug("[BMP] File Size:          {}", file_header->size);
        logger->debug("[BMP] Data Offset:        {}", file_header->bits_offset);
        logger->debug("[BMP] Image Width:        {}", bmp_header->width);
        logger->debug("[BMP] Image Height:       {}", bmp_header->height);
        logger->debug("[BMP] Image Planes:       {}", bmp_header->planes);
        logger->debug("[BMP] Image BitCount:     {}", bmp_header->bit_count);
        logger->debug("[BMP] Image Comperession: {}", bmp_header->compression);
        logger->debug("[BMP] Image Size:         {}", bmp_header->size_image);

        auto width      = std::abs(bmp_header->width);
        auto height     = std::abs(bmp_header->height);
        auto bitcount   = 32;
        auto byte_count = bitcount >> 3;
        auto pitch      = ((width * bitcount >> 3) + 3) & ~3;
        auto data_size  = pitch * height;
        auto img        = std::allocate_shared<Image>(alloc, width, height, bitcount, pitch, data_size);
        auto data       = img->Buffer().Span<math::R8G8B8A8Unorm>();
        if (bitcount < 24) {
            logger->warn("[BMP] Sorry, only true color BMP is supported at now.");
        } else {
            const uint8_t* source_data =
                reinterpret_cast<const uint8_t*>(buf.GetData()) +
                file_header->bits_offset;

            size_t index = 0;
            for (int32_t y = height - 1; y >= 0; y--) {
                for (uint32_t x = 0; x < width; x++) {
                    data[index].bgra = *reinterpret_cast<const R8G8B8A8Unorm*>(
                        source_data + pitch * y + x * byte_count);
                    index++;
                }
            }
        }

        return img;
    }
    return {};
}
}  // namespace hitagi::resource