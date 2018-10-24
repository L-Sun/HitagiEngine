#pragma once
#include <iostream>
#include <array>

namespace My {
typedef struct _Image {
    typedef std::array<uint8_t, 4> bgra;

    uint32_t Width;
    uint32_t Height;
    uint32_t bitcount;
    uint32_t pitch;
    size_t   data_size;

    bgra* data;
} Image;

std::ostream& operator<<(std::ostream& out, const Image& image);
}  // namespace My
