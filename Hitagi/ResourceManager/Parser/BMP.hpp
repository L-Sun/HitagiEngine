#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Resource {
class BmpParser : public ImageParser {
public:
    Image Parse(const Core::Buffer& buf) final;

private:
#pragma pack(push, 1)
    using BITMAP_FILEHEADER = struct _BITMAP_FILEHEADER {

        uint16_t Signature;

        uint32_t Size;

        uint32_t Reserved;

        uint32_t BitsOffset;

    };
#define BITMAP_FILEHEADER_SIZE 14

    using BITMAP_HEADER = struct _BITMAP_HEADER {

        uint32_t HeaderSize;

        int32_t  Width;

        int32_t  Height;

        uint16_t Planes;

        uint16_t BitCount;

        uint32_t Compression;

        uint32_t SizeImage;

        int32_t  PelsPerMeterX;

        int32_t  PelsPerMeterY;

        uint32_t ClrUsed;

        uint32_t ClrImportant;

    };
#pragma pack(pop)
};
}  // namespace Hitagi::Resource
