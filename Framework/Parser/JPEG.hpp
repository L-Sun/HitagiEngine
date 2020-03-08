#pragma once
#include "ImageParser.hpp"

namespace Hitagi {
class JpegParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};

}  // namespace Hitagi
