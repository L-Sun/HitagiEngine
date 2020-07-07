#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Asset {
class TgaParser : public ImageParser {
public:
    Image Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Asset
