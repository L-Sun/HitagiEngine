module;

#include <spdlog/spdlog.h>
#include <png.h>

module asset;
import std;

using namespace hitagi::math;

namespace hitagi::asset {

struct ImageSource {
    const std::byte* data;
    int              size;
    int              offset;
};
void png_read_callback(png_structp png_tr, png_bytep data, png_size_t length) {
    auto isource = reinterpret_cast<ImageSource*>(png_get_io_ptr(png_tr));

    if (isource->offset + length <= isource->size) {
        std::memcpy(data, isource->data + isource->offset, length);
        isource->offset += length;
    } else
        png_error(png_tr, "[libpng] pngReaderCallback failed.");
}

std::shared_ptr<Texture> PngDecoder::Decode(const core::Buffer& buffer) {
    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    if (buffer.Empty()) {
        logger->warn("[PNG] Parsing a empty bufferfer will return nullptr.");
        return nullptr;
    }

    enum { PNG_BYTES_TO_CHECK = 4 };

    if (buffer.GetDataSize() < PNG_BYTES_TO_CHECK ||
        png_sig_cmp(reinterpret_cast<png_const_bytep>(buffer.GetData()), 0,
                    PNG_BYTES_TO_CHECK)) {
        logger->warn("[PNG] File format is not png!");
        return nullptr;
    }

    png_structp png_tr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                nullptr, nullptr);
    if (!png_tr) {
        logger->error("[PNG] Can not create read struct.");
        return nullptr;
    }
    png_infop info_ptr = png_create_info_struct(png_tr);
    if (!info_ptr) {
        logger->error("[PNG] Can not create info struct.");
        png_destroy_read_struct(&png_tr, nullptr, nullptr);
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png_tr))) {
        logger->error("[PNG] Error occur during read_image.");
        png_destroy_read_struct(&png_tr, &info_ptr, nullptr);
        return nullptr;
    }

    ImageSource img_source{};
    img_source.data   = buffer.GetData();
    img_source.size   = buffer.GetDataSize();
    img_source.offset = 0;

    png_set_read_fn(png_tr, &img_source, png_read_callback);
    png_read_png(
        png_tr, info_ptr,
        PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_PACKING,
        nullptr);

    auto width      = png_get_image_width(png_tr, info_ptr);
    auto height     = png_get_image_height(png_tr, info_ptr);
    auto pitch      = ((width * 4) + 3) & ~3;
    auto cpu_buffer = core::Buffer(pitch * height);

    png_bytepp rows = png_get_rows(png_tr, info_ptr);
    auto       p    = reinterpret_cast<R8G8B8A8Unorm*>(cpu_buffer.GetData());

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
                auto q = reinterpret_cast<R8G8B8A8Unorm*>(rows[i]);
                std::copy(q, q + width, p);
                // to next line
                p += width;
            }
        } break;
        default:
            logger->error("[PNG] Unsupport color type.");
            return nullptr;
            break;
    }

    png_destroy_read_struct(&png_tr, &info_ptr, nullptr);
    return std::make_shared<Texture>(width, height, gfx::Format::R8G8B8A8_UNORM, std::move(cpu_buffer));
}

struct PngWriteContext {
    std::pmr::vector<std::byte> data;
};

void png_write_callback(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto* ctx = reinterpret_cast<PngWriteContext*>(png_get_io_ptr(png_ptr));
    auto* src = reinterpret_cast<const std::byte*>(data);
    ctx->data.insert(ctx->data.end(), src, src + length);
}

void png_flush_callback(png_structp) {}

core::Buffer PngEncoder::Encode(const Texture& texture) {
    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    if (texture.Empty()) {
        logger->warn("[PNG] Encoding an empty texture will return empty buffer.");
        return {};
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        logger->error("[PNG] Can not create write struct.");
        return {};
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        logger->error("[PNG] Can not create info struct.");
        png_destroy_write_struct(&png_ptr, nullptr);
        return {};
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        logger->error("[PNG] Error occurred during write_image.");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return {};
    }

    PngWriteContext write_ctx;
    png_set_write_fn(png_ptr, &write_ctx, png_write_callback, png_flush_callback);

    auto width  = texture.Width();
    auto height = texture.Height();

    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    auto pixel_data = texture.GetData();
    auto pitch      = width * 4;

    for (std::uint32_t y = 0; y < height; y++) {
        auto row = reinterpret_cast<const png_byte*>(pixel_data.data() + y * pitch);
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return {write_ctx.data.size(), write_ctx.data.data()};
}

}  // namespace hitagi::asset
