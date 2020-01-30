#pragma once
#include <iostream>
#include <vector>
#include <jpeglib.h>
#include "ImageParser.hpp"

namespace My {

class JpegParser : public ImageParser {
public:
    virtual Image Parse(const Buffer& buf) {
        jpeg_decompress_struct cinfo;
        jpeg_error_mgr         jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, buf.GetData(), buf.GetDataSize());
        jpeg_read_header(&cinfo, true);
        cinfo.out_color_space = JCS_EXT_RGBA;

        Image img;
        img.Width     = static_cast<uint32_t>(cinfo.image_width);
        img.Height    = static_cast<uint32_t>(cinfo.image_height);
        img.bitcount  = 32;
        img.pitch     = ((img.Width * img.bitcount >> 3) + 3) & ~3;
        img.data_size = img.pitch * img.Height;
        img.data      = g_pMemoryManager->Allocate(img.data_size);
        jpeg_start_decompress(&cinfo);

        int buffer_height = 1;
        int row_stride    = cinfo.output_width * cinfo.output_components;

        JSAMPARRAY buffer = new JSAMPROW[buffer_height];
        buffer[0]         = new JSAMPLE[row_stride];
        auto p            = reinterpret_cast<uint8_t*>(img.data);

        while (cinfo.output_scanline < cinfo.output_height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            std::memcpy(p, buffer[0], row_stride);
            p += row_stride;
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        delete[] buffer[0];
        delete[] buffer;

        return img;
    }
};

}  // namespace My
