#pragma once
#include "image_parser.hpp"

namespace hitagi::asset {
class BmpParser : public ImageParser {
public:
    std::shared_ptr<Image> Parse(const core::Buffer& buf) final;

private:
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
};
}  // namespace hitagi::asset
