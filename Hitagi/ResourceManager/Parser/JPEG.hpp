#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Resource {
class JpegParser : public ImageParser {
public:
    JpegParser() : ImageParser(spdlog::stdout_color_st("JpegParser")) {}
    Image Parse(const Core::Buffer& buf) final;
};

}  // namespace Hitagi::Resource
