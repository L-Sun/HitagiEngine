#pragma once
#include <iostream>
#include "ImageParser.hpp"
#include "geommath.hpp"
#include "png.h"

namespace My {

class PngParser : public ImageParser {
private:
    struct ImageSource {
        const uint8_t* data;
        int            size;
        int            offset;
    };
    static void pngReadCallback(png_structp png_ptr, png_bytep data,
                                png_size_t length) {
        auto isource = reinterpret_cast<ImageSource*>(png_get_io_ptr(png_ptr));

        if (isource->offset + length <= isource->size) {
            std::memcpy(data, isource->data + isource->offset, length);
            isource->offset += length;
        } else
            png_error(png_ptr, "[libpng] pngReaderCallback failed.");
    }

public:
    virtual Image Parse(const Buffer& buf) {
        Image img;

        enum { PNG_BYTES_TO_CHECK = 4 };
        char check[PNG_BYTES_TO_CHECK];

        if (buf.GetDataSize() < PNG_BYTES_TO_CHECK &&
            png_sig_cmp(reinterpret_cast<png_const_bytep>(check), 0,
                        PNG_BYTES_TO_CHECK)) {
            std::cerr << "[libpng] the file format is not png." << std::endl;
            return img;
        }

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                     nullptr, nullptr, nullptr);
        if (!png_ptr) {
            std::cerr << "[libpng] libpng can not create read struct."
                      << std::endl;
            return img;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            std::cerr << "[libpng] libpng can not create info struct."
                      << std::endl;
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            return img;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            std::cerr << "[libpng] error during read_image." << std::endl;
            png_destroy_read_struct(&png_ptr, &info_ptr, 0);
            return img;
        }

        ImageSource imgSource;
        imgSource.data   = buf.GetData();
        imgSource.size   = buf.GetDataSize();
        imgSource.offset = 0;

        png_set_read_fn(png_ptr, &imgSource, pngReadCallback);
        png_read_png(png_ptr, info_ptr,
                     PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_EXPAND |
                         PNG_TRANSFORM_PACKING,
                     0);

        img.Width     = png_get_image_width(png_ptr, info_ptr);
        img.Height    = png_get_image_height(png_ptr, info_ptr);
        img.bitcount  = 32;
        img.pitch     = ((img.Width * img.bitcount >> 3) + 3) & ~3;
        img.data_size = img.pitch * img.Height;
        img.data      = g_pMemoryManager->Allocate(img.data_size);

        unsigned   row_bytes = png_get_rowbytes(png_ptr, info_ptr);
        png_bytepp rows      = png_get_rows(png_ptr, info_ptr);
        auto       p         = reinterpret_cast<uint8_t*>(img.data);
        for (int i = 0; i < img.Height; ++i) {
            memcpy(p + (row_bytes * i), rows[i], row_bytes);
        }

        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        return img;
    }
};  // namespace My

}  // namespace My
