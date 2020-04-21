#include "JPEG.hpp"

#include <jpeglib.h>
#include <spdlog/spdlog.h>

namespace Hitagi::Resource {

Image JpegParser::Parse(const Core::Buffer& buf) {
    if (buf.GetDataSize() == 0 || buf.GetData() == nullptr) {
        spdlog::get("ResourceManager")->warn("[JPEG] Parser a empty file, will return empty image.");
        return Image();
    }

    jpeg_decompress_struct cinfo;
    jpeg_error_mgr         jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, buf.GetData(), buf.GetDataSize());
    jpeg_read_header(&cinfo, true);
    cinfo.out_color_space = JCS_EXT_RGBA;

    auto  width    = static_cast<uint32_t>(cinfo.image_width);
    auto  height   = static_cast<uint32_t>(cinfo.image_height);
    auto  bitcount = 32;
    auto  pitch    = ((width * bitcount >> 3) + 3) & ~3;
    auto  dataSize = pitch * height;
    Image img(width, height, bitcount, pitch, dataSize);
    jpeg_start_decompress(&cinfo);

    int buffer_height = 1;
    int row_stride    = cinfo.output_width * cinfo.output_components;

    JSAMPARRAY buffer = new JSAMPROW[buffer_height];
    buffer[0]         = new JSAMPLE[row_stride];
    auto p            = reinterpret_cast<uint8_t*>(img.getData());

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
}  // namespace Hitagi::Resource