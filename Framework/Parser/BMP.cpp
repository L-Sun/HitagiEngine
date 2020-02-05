#include <iostream>
#include "geommath.hpp"
#include "BMP.hpp"

using namespace My;

Image BmpParser::Parse(const Buffer& buf) {
    Image                    img;
    const BITMAP_FILEHEADER* pFileHeader =
        reinterpret_cast<const BITMAP_FILEHEADER*>(buf.GetData());
    const BITMAP_HEADER* pBmpHeader = reinterpret_cast<const BITMAP_HEADER*>(
        buf.GetData() + BITMAP_FILEHEADER_SIZE);
    if (pFileHeader->Signature == 0x4D42 /* 'B''M' */) {
        std::cout << "Asset is Windows BMP file" << std::endl;
        std::cout << "BMP Header" << std::endl;
        std::cout << "-----------------------------------" << std::endl;
        std::cout << "File Size:" << pFileHeader->Size << std::endl;
        std::cout << "Data Offset: " << pFileHeader->BitsOffset << std::endl;
        std::cout << "Image Width: " << pBmpHeader->Width << std::endl;
        std::cout << "Image Height: " << pBmpHeader->Height << std::endl;
        std::cout << "Image Planes: " << pBmpHeader->Planes << std::endl;
        std::cout << "Image BitCount: " << pBmpHeader->BitCount << std::endl;
        std::cout << "Image Comperession: " << pBmpHeader->Compression
                  << std::endl;
        std::cout << "Image Size: " << pBmpHeader->SizeImage << std::endl;

        auto     width      = pBmpHeader->Width;
        auto     height     = pBmpHeader->Height;
        auto     bitcount   = 32;
        auto     byte_count = bitcount >> 3;
        auto     pitch      = ((width * bitcount >> 3) + 3) & ~3;
        auto     data_size  = pitch * height;
        Image    img(width, height, bitcount, pitch, data_size);
        uint8_t* data = reinterpret_cast<uint8_t*>(img.getData());
        if (bitcount < 24) {
            std::cout << "Sorry, only true color BMP is supported at now."
                      << std::endl;
        } else {
            const uint8_t* pSourceData =
                reinterpret_cast<const uint8_t*>(buf.GetData()) +
                pFileHeader->BitsOffset;
            for (int32_t y = height - 1; y >= 0; y--) {
                for (uint32_t x = 0; x < width; x++) {
                    (reinterpret_cast<R8G8B8A8Unorm*>(data +
                                                      pitch * (height - y - 1) + x * byte_count))
                        ->bgra = *reinterpret_cast<const R8G8B8A8Unorm*>(
                        pSourceData + pitch * y + x * byte_count);
                }
            }
        }
    }
    return img;
}
