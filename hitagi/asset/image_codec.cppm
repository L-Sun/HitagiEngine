module;

#include <spdlog/logger.h>

export module asset:image_codec;
import std;
import :texture;
import core;

export namespace hitagi::asset {

enum struct ImageFormat : std::uint8_t {
    UNKOWN,
    PNG,
    JPEG,
    TGA,
    BMP,
};

inline constexpr ImageFormat get_image_format(std::string_view ext) noexcept {
    if (ext == ".jpeg" || ext == ".jpg")
        return ImageFormat::JPEG;
    else if (ext == ".bmp")
        return ImageFormat::BMP;
    else if (ext == ".tga")
        return ImageFormat::TGA;
    else if (ext == ".png")
        return ImageFormat::PNG;
    return ImageFormat::UNKOWN;
}

class ImageDecoder {
public:
    ImageDecoder(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Decode(const std::filesystem::path& path) -> std::shared_ptr<Texture>;
    virtual auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> = 0;
    virtual ~ImageDecoder()                                                     = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class ImageEncoder {
public:
    ImageEncoder(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Encode(const Texture& texture, const std::filesystem::path& path) -> bool;
    virtual auto Encode(const Texture& texture) -> core::Buffer = 0;
    virtual ~ImageEncoder()                                     = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class PngDecoder : public ImageDecoder {
public:
    using ImageDecoder::ImageDecoder;
    using ImageDecoder::Decode;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class PngEncoder : public ImageEncoder {
public:
    using ImageEncoder::ImageEncoder;
    using ImageEncoder::Encode;
    auto Encode(const Texture& texture) -> core::Buffer final;
};

class JpegDecoder : public ImageDecoder {
public:
    using ImageDecoder::ImageDecoder;
    using ImageDecoder::Decode;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class BmpDecoder : public ImageDecoder {
public:
    using ImageDecoder::ImageDecoder;
    using ImageDecoder::Decode;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class TgaDecoder : public ImageDecoder {
public:
    using ImageDecoder::ImageDecoder;
    using ImageDecoder::Decode;
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

}  // namespace hitagi::asset
