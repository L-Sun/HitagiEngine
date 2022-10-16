#pragma once
#include "image_parser.hpp"

namespace hitagi::asset {
class PngParser : public ImageParser {
public:
    std::shared_ptr<Texture> Parse(const core::Buffer& buf) final;
};
}  // namespace hitagi::asset
