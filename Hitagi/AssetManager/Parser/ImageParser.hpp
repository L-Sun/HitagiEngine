#pragma once

#include "../Image.hpp"
#include "Buffer.hpp"

namespace Hitagi::Asset {
enum class ImageFormat : unsigned { PNG,
                                    JPEG,
                                    TGA,
                                    BMP,
                                    NUM_SUPPORT };

class ImageParser {
public:
    virtual Image Parse(const Core::Buffer& buf) = 0;
    virtual ~ImageParser()                       = default;
};
}  // namespace Hitagi::Asset
