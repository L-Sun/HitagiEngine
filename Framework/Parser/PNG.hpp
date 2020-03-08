#pragma once
#include "ImageParser.hpp"

namespace Hitagi {
class PngParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};
}  // namespace Hitagi
