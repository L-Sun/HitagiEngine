#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/asset/texture.hpp>

namespace hitagi::asset {

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
}  // namespace hitagi::asset
