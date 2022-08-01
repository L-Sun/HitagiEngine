#include <hitagi/parser/jpeg.hpp>

#include <jpeglib.h>
#include <spdlog/spdlog.h>

#include <string>

namespace hitagi::resource {

std::optional<Texture> JpegParser::Parse(const core::Buffer& buf) {
    auto logger = spdlog::get("AssetManager");
    if (buf.Empty()) {
        logger->warn("[JPEG] Parsing a empty buffer will return nullptr");
        return std::nullopt;
    }
    Texture                image;
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr         jerr{};
    cinfo.err       = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr cinfo) { throw cinfo->err; };

    try {
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, reinterpret_cast<const std::uint8_t*>(buf.GetData()), buf.GetDataSize());
        jpeg_read_header(&cinfo, true);
        cinfo.out_color_space = JCS_EXT_RGBA;

        image.width      = static_cast<std::uint32_t>(cinfo.image_width);
        image.height     = static_cast<std::uint32_t>(cinfo.image_height);
        image.format     = Format::R8G8B8A8_UNORM;
        image.pitch      = ((image.width * 4) + 3) & ~3;
        image.cpu_buffer = core::Buffer(image.pitch * image.height);
        jpeg_start_decompress(&cinfo);

        int row_stride = cinfo.output_width * cinfo.output_components;

        auto row_buffer = std::pmr::vector<JSAMPLE>(row_stride);
        auto p          = image.cpu_buffer.GetData() + (image.height - 1) * row_stride;

        while (cinfo.output_scanline < cinfo.output_height) {
            auto p_row = row_buffer.data();
            jpeg_read_scanlines(&cinfo, &p_row, 1);
            std::memcpy(p, p_row, row_stride);
            p -= row_stride;
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

    } catch (struct jpeg_error_mgr* err) {
        std::array<char, 1024> error_message;
        (cinfo.err->format_message)((j_common_ptr)&cinfo, error_message.data());

        logger->error(error_message.data());
    }
    return image;
}
}  // namespace hitagi::resource