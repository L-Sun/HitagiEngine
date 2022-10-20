#pragma once
#include <hitagi/asset/parser/image_parser.hpp>

namespace hitagi::asset {
class JpegParser : public ImageParser {
public:
    using ImageParser::ImageParser;

    using ImageParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Texture> final;
};

}  // namespace hitagi::asset
