#pragma once
#include <iostream>

namespace My {

class TgaParser : public ImageParser {
public:
    virtual Image Parse(const Buffer& buf);
};
}  // namespace My
