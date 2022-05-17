#include <hitagi/resource/image.hpp>

namespace hitagi::resource {

Image::Image(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t bitcount,
    std::uint32_t pitch,
    std::size_t   data_size)
    : m_Width(width),
      m_Height(height),
      m_Bitcount(bitcount),
      m_Pitch(pitch),
      m_Buffer(data_size) {
}

}  // namespace hitagi::resource
