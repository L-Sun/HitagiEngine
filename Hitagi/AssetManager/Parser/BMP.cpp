#include "BMP.hpp"

#include <spdlog/spdlog.h>

#include <Math/Vector.hpp>

using namespace Hitagi::Math;

namespace Hitagi::Asset {

std::shared_ptr<Image> BmpParser::Parse(const Core::Buffer& buf) {
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
        auto img        = std::make_shared<Image>(width, height, bitcount, pitch, data_size);
        auto data       = reinterpret_cast<R8G8B8A8Unorm*>(img->GetData());
        if (bitcount < 24) {
            logger->warn("[BMP] Sorry, only true color BMP is supported at now.");
        } else {
            const uint8_t* source_data =
                reinterpret_cast<const uint8_t*>(buf.GetData()) +
                file_header->bits_offset;
            for (int32_t y = height - 1; y >= 0; y--) {
                for (uint32_t x = 0; x < width; x++) {
                    data->bgra = *reinterpret_cast<const R8G8B8A8Unorm*>(
                        source_data + pitch * y + x * byte_count);
                    data++;
                }
            }
        }

        return img;
    }
    return {};
}
}  // namespace Hitagi::Asset