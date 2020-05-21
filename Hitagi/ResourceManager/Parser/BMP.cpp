#include "BMP.hpp"

#include <spdlog/spdlog.h>

#include "HitagiMath.hpp"

namespace Hitagi::Resource {

Image BmpParser::Parse(const Core::Buffer& buf) {
    const auto* fileHeader =
        reinterpret_cast<const BITMAP_FILEHEADER*>(buf.GetData());
    const auto* bmpHeader = reinterpret_cast<const BITMAP_HEADER*>(
        buf.GetData() + BITMAP_FILEHEADER_SIZE);
    if (fileHeader->Signature == 0x4D42 /* 'B''M' */) {
        spdlog::get("ResourceManager")->debug("[BMP] Asset is Windows BMP file");
        spdlog::get("ResourceManager")->debug("[BMP] BMP Header");
        spdlog::get("ResourceManager")->debug("[BMP] -----------------------------------");
        spdlog::get("ResourceManager")->debug("[BMP] File Size:          {}", fileHeader->Size);
        spdlog::get("ResourceManager")->debug("[BMP] Data Offset:        {}", fileHeader->BitsOffset);
        spdlog::get("ResourceManager")->debug("[BMP] Image Width:        {}", bmpHeader->Width);
        spdlog::get("ResourceManager")->debug("[BMP] Image Height:       {}", bmpHeader->Height);
        spdlog::get("ResourceManager")->debug("[BMP] Image Planes:       {}", bmpHeader->Planes);
        spdlog::get("ResourceManager")->debug("[BMP] Image BitCount:     {}", bmpHeader->BitCount);
        spdlog::get("ResourceManager")->debug("[BMP] Image Comperession: {}", bmpHeader->Compression);
        spdlog::get("ResourceManager")->debug("[BMP] Image Size:         {}", bmpHeader->SizeImage);

        auto     width      = std::abs(bmpHeader->Width);
        auto     height     = std::abs(bmpHeader->Height);
        auto     bitcount   = 32;
        auto     byte_count = bitcount >> 3;
        auto     pitch      = ((width * bitcount >> 3) + 3) & ~3;
        auto     dataSize   = pitch * height;
        Image    img(width, height, bitcount, pitch, dataSize);
        auto* data = reinterpret_cast<uint8_t*>(img.getData());
        if (bitcount < 24) {
            spdlog::get("ResourceManager")->warn("[BMP] Sorry, only true color BMP is supported at now.");
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

        return img;
    }
    return Image();
}
}  // namespace Hitagi::Resource