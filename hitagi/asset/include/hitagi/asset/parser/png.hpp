#pragma once
#include "image_parser.hpp"

namespace hitagi::asset {
class PngParser : public ImageParser {
public:
    using ImageParser::ImageParser;

    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};
}  // namespace hitagi::asset
