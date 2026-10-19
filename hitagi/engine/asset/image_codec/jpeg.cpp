module asset;
import interop.jpeg;
import interop.spdlog;
import std;
import :image_codec;

namespace hitagi::asset {

auto JpegDecoder::DecodeImageData(const core::Buffer& buffer) -> ImageData {
    auto logger = spdlog::default_logger();

    if (buffer.Empty()) {
        logger->warn("[JPEG] Parsing a empty buffer will return nullptr");
        return {};
    }

    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr         jerr{};
    cinfo.err       = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr cinfo) { throw cinfo->err; };

    try {
        interop::create_jpeg_decompress(&cinfo);
        jpeg_mem_src(&cinfo, reinterpret_cast<const std::uint8_t*>(buffer.GetData()), buffer.GetDataSize());
        jpeg_read_header(&cinfo, true);
        cinfo.out_color_space = JCS_EXT_RGBA;

        auto width      = static_cast<std::uint32_t>(cinfo.image_width);
        auto height     = static_cast<std::uint32_t>(cinfo.image_height);
        auto pitch      = ((width * 4) + 3) & ~3;
        auto cpu_buffer = core::Buffer(pitch * height);
        jpeg_start_decompress(&cinfo);

        int row_stride = cinfo.output_width * cinfo.output_components;

        auto row_buffer = std::pmr::vector<JSAMPLE>(row_stride);
        auto p          = cpu_buffer.GetData() + (height - 1) * row_stride;

        while (cinfo.output_scanline < cinfo.output_height) {
            auto p_row = row_buffer.data();
            jpeg_read_scanlines(&cinfo, &p_row, 1);
            std::memcpy(p, p_row, row_stride);
            p -= row_stride;
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        return ImageData{
            .width  = width,
            .height = height,
            .format = gfx::Format::R8G8B8A8_UNORM,
            .data   = std::move(cpu_buffer),
        };

    } catch (struct jpeg_error_mgr* err) {
        std::array<char, 1024> error_message;
        cinfo.err->format_message(reinterpret_cast<j_common_ptr>(&cinfo), error_message.data());

        logger->error(error_message.data());
    }
    return {};
}

auto JpegDecoder::EncodeImageData(const ImageData&) -> core::Buffer {
    return {};
}
}  // namespace hitagi::asset
