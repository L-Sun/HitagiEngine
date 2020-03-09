#pragma once
#include "Image.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
class ImageParser {
public:
    virtual Image Parse(const Core::Buffer& buf) = 0;
};
}  // namespace Hitagi::Resource
