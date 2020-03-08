#pragma once
#include "Image.hpp"
#include "Buffer.hpp"

namespace Hitagi {
class ImageParser {
public:
    virtual Image Parse(const Buffer& buf) = 0;
};
}  // namespace Hitagi
