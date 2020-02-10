#pragma once
#include "ImageParser.hpp"

namespace My {
class JpegParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};

}  // namespace My
