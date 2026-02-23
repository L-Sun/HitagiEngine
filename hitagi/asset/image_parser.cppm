module;

#include <spdlog/logger.h>

export module asset:image_parser;
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

class ImageParser {
public:
    ImageParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Parse(const std::filesystem::path& path) -> std::shared_ptr<Texture>;
    virtual auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> = 0;
    virtual ~ImageParser()                                                     = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class PngParser : public ImageParser {
public:
    using ImageParser::ImageParser;
    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class JpegParser : public ImageParser {
public:
    using ImageParser::ImageParser;
    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class BmpParser : public ImageParser {
public:
    using ImageParser::ImageParser;
    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

class TgaParser : public ImageParser {
public:
    using ImageParser::ImageParser;
    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

}  // namespace hitagi::asset
