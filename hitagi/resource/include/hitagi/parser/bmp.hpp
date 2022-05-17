#pragma once
#include "image_parser.hpp"

namespace hitagi::resource {
class BmpParser : public ImageParser {
public:
    std::shared_ptr<resource::Image> Parse(const core::Buffer& buf) final;
};
}  // namespace hitagi::resource
