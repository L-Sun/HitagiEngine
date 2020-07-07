#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Asset {
class JpegParser : public ImageParser {
public:
    Image Parse(const Core::Buffer& buf) final;
};

}  // namespace Hitagi::Asset
