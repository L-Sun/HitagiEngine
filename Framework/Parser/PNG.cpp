#include <iostream>
#include "geommath.hpp"
#include "png.h"
#include "PNG.hpp"

using namespace My;
struct ImageSource {
    const uint8_t* data;
    int            size;
    int            offset;
};
void pngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto isource = reinterpret_cast<ImageSource*>(png_get_io_ptr(png_ptr));

    if (isource->offset + length <= isource->size) {
        std::memcpy(data, isource->data + isource->offset, length);
        isource->offset += length;
    } else
        png_error(png_ptr, "[libpng] pngReaderCallback failed.");
}

Image PngParser::Parse(const Buffer& buf) {
    enum { PNG_BYTES_TO_CHECK = 4 };
    char check[PNG_BYTES_TO_CHECK];

    if (buf.GetDataSize() < PNG_BYTES_TO_CHECK &&
        png_sig_cmp(reinterpret_cast<png_const_bytep>(check), 0,
                    PNG_BYTES_TO_CHECK)) {
        std::cerr << "[libpng] the file format is not png." << std::endl;
        return Image();
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                 nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "[libpng] libpng can not create read struct." << std::endl;
        return Image();
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "[libpng] libpng can not create info struct." << std::endl;
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return Image();
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "[libpng] error during read_image." << std::endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        return Image();
    }

    ImageSource imgSource;
    imgSource.data   = buf.GetData();
    imgSource.size   = buf.GetDataSize();
    imgSource.offset = 0;

    png_set_read_fn(png_ptr, &imgSource, pngReadCallback);
    png_read_png(
        png_ptr, info_ptr,
        PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_PACKING,
        0);

    auto  width    = png_get_image_width(png_ptr, info_ptr);
    auto  height   = png_get_image_height(png_ptr, info_ptr);
    auto  bitcount = 32;
    auto  pitch    = ((width * bitcount >> 3) + 3) & ~3;
    auto  dataSize = pitch * height;
    Image img(width, height, bitcount, pitch, dataSize);

    png_bytepp rows = png_get_rows(png_ptr, info_ptr);
    auto       p    = reinterpret_cast<R8G8B8A8Unorm*>(img.getData());

    switch (png_get_color_type(png_ptr, info_ptr)) {
        case PNG_COLOR_TYPE_GRAY: {
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    p[j].r = rows[i][j];
                    p[j].g = rows[i][j];
                    p[j].b = rows[i][j];
                    p[j].a = 255;
                }
                // to next line
                p += width;
            }
        } break;
        case PNG_COLOR_TYPE_GRAY_ALPHA: {
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    p[j].r = rows[i][2 * j + 0];
                    p[j].g = rows[i][2 * j + 0];
                    p[j].b = rows[i][2 * j + 0];
                    p[j].a = rows[i][2 * j + 1];
                }
                // to next line
                p += width;
            }
        } break;
        case PNG_COLOR_TYPE_RGB: {
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    p[j].r = rows[i][3 * j + 0];
                    p[j].g = rows[i][3 * j + 1];
                    p[j].b = rows[i][3 * j + 2];
                    p[j].a = 255;
                }
                // to next line
                p += width;
            }
        } break;
        case PNG_COLOR_TYPE_RGBA: {
            for (int i = 0; i < height; i++) {
                memcpy(p[i * width], rows[i], pitch);
            }
        } break;
        default:
            std::cerr << "[libpng] Unsupport color type." << std::endl;
            return img;
            break;
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    return img;
}