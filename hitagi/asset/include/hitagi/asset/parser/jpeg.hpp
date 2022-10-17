#pragma once
#include <hitagi/asset/parser/image_parser.hpp>

namespace hitagi::asset {
class JpegParser : public ImageParser {
public:
    using ImageParser::ImageParser;

    std::shared_ptr<Texture> Parse(const core::Buffer& buf) final;
};

}  // namespace hitagi::asset
