#include "PNG.hpp"

#include <iostream>

#include <spdlog/spdlog.h>
#include <png.h>

#include "HitagiMath.hpp"

namespace Hitagi::Resource {

struct ImageSource {
    const uint8_t* data;
    int            size;
    int            offset;
};
void pngReadCallback(png_structp png_tr, png_bytep data, png_size_t length) {
    auto isource = reinterpret_cast<ImageSource*>(png_get_io_ptr(png_tr));

    if (isource->offset + length <= isource->size) {
        std::memcpy(data, isource->data + isource->offset, length);
        isource->offset += length;
    } else
        png_error(png_tr, "[libpng] pngReaderCallback failed.");
}

Image PngParser::Parse(const Core::Buffer& buf) {
    auto logger = spdlog::get("ResourceManager");
    if (buf.Empty()) {
        logger->warn("[PNG] Parsing a empty buffer will return empty image.");
        return Image{};
    }

    enum { PNG_BYTES_TO_CHECK = 4 };

    if (buf.GetDataSize() < PNG_BYTES_TO_CHECK ||
        png_sig_cmp(reinterpret_cast<png_const_bytep>(buf.GetData()), 0,
                    PNG_BYTES_TO_CHECK)) {
        logger->warn("[PNG] File format is not png!");
        return Image();
    }

    png_structp png_tr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                nullptr, nullptr);
    if (!png_tr) {
        logger->error("[PNG] Can not create read struct.");
        return Image();
    }
    png_infop info_ptr = png_create_info_struct(png_tr);
    if (!info_ptr) {
        logger->error("[PNG] Can not create info struct.");
        png_destroy_read_struct(&png_tr, nullptr, nullptr);
        return Image();
    }

    if (setjmp(png_jmpbuf(png_tr))) {
        logger->error("[PNG] Error occur during read_image.");
        png_destroy_read_struct(&png_tr, &info_ptr, nullptr);
        return Image();
    }

    ImageSource imgSource;
    imgSource.data   = buf.GetData();
    imgSource.size   = buf.GetDataSize();
    imgSource.offset = 0;

    png_set_read_fn(png_tr, &imgSource, pngReadCallback);
    png_read_png(
        png_tr, info_ptr,
        PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_PACKING,
        nullptr);

    auto  width    = png_get_image_width(png_tr, info_ptr);
    auto  height   = png_get_image_height(png_tr, info_ptr);
    auto  bitcount = 32;
    auto  pitch    = ((width * bitcount >> 3) + 3) & ~3;
    auto  dataSize = pitch * height;
    Image img(width, height, bitcount, pitch, dataSize);

    png_bytepp rows = png_get_rows(png_tr, info_ptr);
    auto       p    = reinterpret_cast<R8G8B8A8Unorm*>(img.GetData());

    switch (png_get_color_type(png_tr, info_ptr)) {
        case PNG_COLOR_TYPE_GRAY: {
            for (int i = height - 1; i >= 0; i--) {
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
            for (int i = height - 1; i >= 0; i--) {
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
            for (int i = height - 1; i >= 0; i--) {
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
            for (int i = height - 1; i >= 0; i--) {
                auto _p = reinterpret_cast<R8G8B8A8Unorm*>(rows[i]);
                std::copy(_p, _p + width, p);
                // to next line
                p += width;
            }
        } break;
        default:
            logger->error("[PNG] Unsupport color type.");
            return img;
            break;
    }

    png_destroy_read_struct(&png_tr, &info_ptr, nullptr);
    return img;
}
}  // namespace Hitagi::Resource