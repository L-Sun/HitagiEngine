#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Resource {
class PngParser : public ImageParser {
public:
    PngParser() : ImageParser(spdlog::stdout_color_st("PngParser")) {}
    Image Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource
