#pragma once
#include "image_parser.hpp"

namespace hitagi::asset {
class BmpParser : public ImageParser {
public:
    using ImageParser::ImageParser;

    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};
}  // namespace hitagi::asset
