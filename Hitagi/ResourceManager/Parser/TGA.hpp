#pragma once
#include <iostream>

namespace Hitagi::Resource {
class TgaParser : public ImageParser {
public:
    Image Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource
