#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/core/buffer.hpp>

namespace hitagi::resource {
class Image : public ResourceObject {
public:
    Image(std::uint32_t width,
          std::uint32_t height,
          std::uint32_t bitcount,
          std::uint32_t pitch,
          std::size_t   data_size);

    inline std::uint32_t Width() const noexcept { return m_Width; }
    inline std::uint32_t Height() const noexcept { return m_Height; }
    inline std::uint32_t Bitcount() const noexcept { return m_Bitcount; }
    inline std::uint32_t Pitch() const noexcept { return m_Pitch; }
    auto&                Buffer() noexcept { return m_Buffer; }

private:
    std::uint32_t m_Width    = 0;
    std::uint32_t m_Height   = 0;
    std::uint32_t m_Bitcount = 0;
    std::uint32_t m_Pitch    = 0;

    core::Buffer m_Buffer;
};

}  // namespace hitagi::resource
