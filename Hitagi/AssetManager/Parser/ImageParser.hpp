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
    virtual std::shared_ptr<Image> Parse(const Core::Buffer& buffer) = 0;
    virtual ~ImageParser()                                        = default;
};
}  // namespace Hitagi::Asset
