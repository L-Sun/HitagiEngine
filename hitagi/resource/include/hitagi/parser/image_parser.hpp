#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/image.hpp>

namespace hitagi::asset {
enum class ImageFormat : unsigned {
    PNG,
    JPEG,
    TGA,
    BMP,
    NUM_SUPPORT
};

inline ImageFormat get_image_format(std::string_view ext) {
    if (ext == ".jpeg" || ext == ".jpg")
        return ImageFormat::JPEG;
    else if (ext == ".bmp")
        return ImageFormat::BMP;
    else if (ext == ".tga")
        return ImageFormat::TGA;
    else if (ext == ".png")
        return ImageFormat::PNG;
    return ImageFormat::NUM_SUPPORT;
}

class ImageParser {
public:
    virtual std::shared_ptr<Image> Parse(const core::Buffer& buffer) = 0;
    virtual ~ImageParser()                                           = default;
};
}  // namespace hitagi::asset
