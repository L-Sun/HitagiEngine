#pragma once
#include <iostream>

namespace My {

class TgaParser : public ImageParser {
public:
    Image Parse(const Buffer& buf) final;
};
}  // namespace My
