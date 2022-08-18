#pragma once
#include "image_parser.hpp"

namespace hitagi::resource {
class JpegParser : public ImageParser {
public:
    std::shared_ptr<Texture> Parse(const core::Buffer& buf) final;
};

}  // namespace hitagi::resource
