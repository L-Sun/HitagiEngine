#pragma once
#include "ImageParser.hpp"

namespace My {
class PngParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};
}  // namespace My
