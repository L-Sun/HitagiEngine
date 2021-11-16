#include "Image.hpp"

#include <fmt/format.h>
#include <array>

#include "MemoryManager.hpp"

namespace Hitagi::Asset {

Image::Image(uint32_t width, uint32_t height, uint32_t bitcount, uint32_t pitch, size_t data_size)
    : m_Width(width),
      m_Height(height),
      m_Bitcount(bitcount),
      m_Pitch(pitch),
      Core::Buffer(data_size) {
}

auto operator<<(std::ostream& out, const Image& image) -> std::ostream& {
    double               size = image.GetDataSize();
    constexpr std::array unit = {"B", "kB", "MB"};
    size_t               i    = 0;
    for (; i < 2; i++) {
        if (size < 10) break;
        size /= 1024;
    }

    return out << fmt::format(
               "{0:-^25}\n"
               "Width     {1:>10} px\n"
               "Height    {2:>10} px\n"
               "Bit Count {3:>10} bits\n"
               "Pitch     {4:>10} \n"
               "Data Size {5:>10.2f} {6}\n"
               "{7:-^25}",
               "Image Info", image.m_Width, image.m_Height, image.m_Bitcount, image.m_Pitch, size, unit[i], "End");
}
}  // namespace Hitagi::Asset
