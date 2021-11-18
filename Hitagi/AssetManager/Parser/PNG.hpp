#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Asset {
class PngParser : public ImageParser {
public:
    std::shared_ptr<Image> Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Asset
