#pragma once
#include "Image.hpp"
#include "Buffer.hpp"

namespace My {
class ImageParser {
public:
    virtual Image Parse(const Buffer& buf) = 0;
};
}  // namespace My
