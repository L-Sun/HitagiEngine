#pragma once
#include <iostream>

namespace Hitagi {

class TgaParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};
}  // namespace Hitagi
