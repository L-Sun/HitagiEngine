#pragma once
#include "image_parser.hpp"

namespace hitagi::resource {
class PngParser : public ImageParser {
public:
    std::shared_ptr<Image> Parse(const core::Buffer& buf) final;
};
}  // namespace hitagi::resource
