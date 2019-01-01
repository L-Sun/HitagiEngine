#pragma once
#include <iostream>

namespace My {
struct Image {
    uint32_t Width;
    uint32_t Height;
    uint32_t bitcount;
    uint32_t pitch;
    size_t   data_size;

    void* data;
};

std::ostream& operator<<(std::ostream& out, const Image& image);
}  // namespace My
