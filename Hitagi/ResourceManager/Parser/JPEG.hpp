#pragma once
#include "../ImageParser.hpp"

namespace Hitagi::Resource {
class JpegParser : public ImageParser {
public:
    Image Parse(const Core::Buffer& buf) final;
};

}  // namespace Hitagi::Resource
