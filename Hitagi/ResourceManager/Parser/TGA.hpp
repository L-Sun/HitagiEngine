#pragma once
#include "ImageParser.hpp"

namespace Hitagi::Resource {
class TgaParser : public ImageParser {
public:
    TgaParser() : ImageParser(spdlog::stdout_color_st("TgaParser")) {}
    Image Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource
