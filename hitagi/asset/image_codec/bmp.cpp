module asset;
import interop.spdlog;
import std;
import math;
import :image_codec;

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

auto BmpDecoder::DecodeImageData(const core::Buffer& buffer) -> ImageData {
    auto logger = spdlog::default_logger();
    if (buffer.Empty()) {
        logger->warn("[BMP] Parsing a empty buffer will return nullptr");
        return {};
    }

    auto file_header = reinterpret_cast<const BITMAP_FILEHEADER*>(buffer.GetData());
    auto bmp_header  = reinterpret_cast<const BITMAP_HEADER*>(buffer.GetData() + BITMAP_FILEHEADER_SIZE);
    if (file_header->signature == 0x4D42 /* 'B''M' */) {
        logger->trace("[BMP] Asset is Windows BMP file");
        logger->trace("[BMP] BMP Header");
        logger->trace("[BMP] -----------------------------------");
        logger->trace("[BMP] File Size:          {}", file_header->size);
        logger->trace("[BMP] Data Offset:        {}", file_header->bits_offset);
        logger->trace("[BMP] Image Width:        {}", bmp_header->width);
        logger->trace("[BMP] Image Height:       {}", bmp_header->height);
        logger->trace("[BMP] Image Planes:       {}", bmp_header->planes);
        logger->trace("[BMP] Image BitCount:     {}", bmp_header->bit_count);
        logger->trace("[BMP] Image Compression: {}", bmp_header->compression);
        logger->trace("[BMP] Image Size:         {}", bmp_header->size_image);

        if (bmp_header->bit_count < 24) {
            logger->warn("[BMP] Sorry, only true color BMP is supported at now.");
            return {};
        }

        const auto width  = static_cast<std::uint32_t>(std::abs(bmp_header->width));
        const auto height = static_cast<std::uint32_t>(std::abs(bmp_header->height));
        // BMP rows are stored bottom-up, padded to 4 bytes, at bit_count/8 bytes per pixel.
        const std::size_t src_bpp   = bmp_header->bit_count / 8;
        const std::size_t src_pitch = ((width * src_bpp) + 3) & ~std::size_t{3};
        const std::size_t src_size  = src_pitch * height;
        if (file_header->bits_offset + src_size > buffer.GetDataSize()) {
            logger->warn("[BMP] Pixel data ({} bytes at offset {}) exceeds buffer size ({})", src_size, file_header->bits_offset, buffer.GetDataSize());
            return {};
        }

        auto cpu_buffer = core::Buffer(sizeof(R8G8B8A8Unorm) * width * height);
        auto dest_data  = cpu_buffer.Span<R8G8B8A8Unorm>();

        const auto* source_data = reinterpret_cast<const std::uint8_t*>(buffer.GetData()) + file_header->bits_offset;
        std::size_t index       = 0;
        for (std::uint32_t y = height; y-- > 0;) {
            const auto* row = source_data + src_pitch * y;
            for (std::uint32_t x = 0; x < width; x++) {
                const auto* px    = row + x * src_bpp;
                dest_data[index] = R8G8B8A8Unorm{px[2], px[1], px[0], src_bpp == 4 ? px[3] : std::uint8_t{255}};
                index++;
            }
        }

        return ImageData{
            .width  = static_cast<std::uint32_t>(width),
            .height = static_cast<std::uint32_t>(height),
            .format = gfx::Format::R8G8B8A8_UNORM,
            .data   = std::move(cpu_buffer),
        };
    }
    return {};
}

auto BmpDecoder::EncodeImageData(const ImageData&) -> core::Buffer {
    return {};
}
}  // namespace hitagi::asset
