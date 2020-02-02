#pragma once
#include "ImageParser.hpp"

namespace My {
class JpegParser : public ImageParser {
public:
    virtual Image Parse(const Buffer& buf);
};

}  // namespace My
