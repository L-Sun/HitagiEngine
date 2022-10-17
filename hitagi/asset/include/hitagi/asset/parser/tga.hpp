#pragma once
#include "image_parser.hpp"

namespace hitagi::asset {
class TgaParser : public ImageParser {
public:
    using ImageParser::ImageParser;

    std::shared_ptr<Texture> Parse(const core::Buffer& buf, const std::filesystem::path& path = {}) final;
};
}  // namespace hitagi::asset
