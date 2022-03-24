#pragma once
#include "ImageParser.hpp"

namespace hitagi::asset {
class TgaParser : public ImageParser {
public:
    std::shared_ptr<Image> Parse(const core::Buffer& buf) final;
};
}  // namespace hitagi::asset
