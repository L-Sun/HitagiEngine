#include <hitagi/parser/jpeg.hpp>

#include <jpeglib.h>
#include <spdlog/spdlog.h>

#include <string>

namespace hitagi::resource {

std::shared_ptr<Image> JpegParser::Parse(const core::Buffer& buf) {
    auto logger = spdlog::get("AssetManager");
    if (buf.Empty()) {
        logger->warn("[JPEG] Parsing a empty buffer will return nullptr");
        return nullptr;
    }
    std::shared_ptr<Image> img;
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr         jerr{};
    cinfo.err       = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr cinfo) { throw cinfo->err; };

    try {
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, reinterpret_cast<const std::uint8_t*>(buf.GetData()), buf.GetDataSize());
        jpeg_read_header(&cinfo, true);
        cinfo.out_color_space = JCS_EXT_RGBA;

        auto width     = static_cast<uint32_t>(cinfo.image_width);
        auto height    = static_cast<uint32_t>(cinfo.image_height);
        auto bitcount  = 32;
        auto pitch     = ((width * bitcount >> 3) + 3) & ~3;
        auto data_size = pitch * height;
        img            = std::make_shared<Image>(width, height, bitcount, pitch, data_size);
        jpeg_start_decompress(&cinfo);

        int buffer_height = 1;
        int row_stride    = cinfo.output_width * cinfo.output_components;

        auto buffer = new JSAMPROW[buffer_height];
        buffer[0]   = new JSAMPLE[row_stride];
        auto p      = reinterpret_cast<uint8_t*>(img->Buffer().GetData()) + (height - 1) * row_stride;
        while (cinfo.output_scanline < cinfo.output_height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            std::memcpy(p, buffer[0], row_stride);
            p -= row_stride;
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        delete[] buffer[0];
        delete[] buffer;
    } catch (struct jpeg_error_mgr* err) {
        std::array<char, 1024> error_message;
        (cinfo.err->format_message)((j_common_ptr)&cinfo, error_message.data());

        logger->error(error_message.data());
    }
    return img;
}
}  // namespace hitagi::resource