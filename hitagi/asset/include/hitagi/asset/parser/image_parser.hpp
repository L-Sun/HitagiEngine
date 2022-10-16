#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/texture.hpp>

namespace hitagi::resource {
enum struct ImageFormat : std::uint8_t {
    UNKOWN,
    PNG,
    JPEG,
    TGA,
    BMP,
};

constexpr inline ImageFormat get_image_format(std::string_view ext) {
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
    virtual std::shared_ptr<Texture> Parse(const core::Buffer& buffer) = 0;
    virtual ~ImageParser()                                             = default;
};
}  // namespace hitagi::resource
