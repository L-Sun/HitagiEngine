#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Resource {
class BmpParser : public ImageParser {
public:
    virtual Image Parse(const Core::Buffer& buf);
    BmpParser() : ImageParser(spdlog::stdout_color_st("BmpParser")) {}

private:
#pragma pack(push, 1)
    typedef struct _BITMAP_FILEHEADER {
        uint16_t Signature;
        uint32_t Size;
        uint32_t Reserved;
        uint32_t BitsOffset;
    } BITMAP_FILEHEADER;
#define BITMAP_FILEHEADER_SIZE 14

    typedef struct _BITMAP_HEADER {
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
    } BITMAP_HEADER;
#pragma pack(pop)
};
}  // namespace Hitagi::Resource
