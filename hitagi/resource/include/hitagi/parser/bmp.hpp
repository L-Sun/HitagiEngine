#pragma once
#include "image_parser.hpp"

namespace hitagi::resource {
class BmpParser : public ImageParser {
public:
    std::optional<Texture> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::resource
