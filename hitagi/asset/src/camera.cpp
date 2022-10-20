#include <hitagi/asset/camera.hpp>

namespace hitagi::asset {

auto Camera::GetViewPort(std::uint32_t screen_width, std::uint32_t screen_height) const noexcept -> gfx::ViewPort {
    gfx::ViewPort view_port;
    {
        std::uint32_t height = screen_height;
        std::uint32_t width  = height * parameters.aspect;
        if (width > screen_width) {
            width     = screen_width;
            height    = screen_width / parameters.aspect;
            view_port = {
                0,
                static_cast<float>((screen_height - height) >> 1),
                static_cast<float>(width),
                static_cast<float>(height),
            };
        } else {
            view_port = {
                static_cast<float>((screen_width - width) >> 1),
                0,
                static_cast<float>(width),
                static_cast<float>(height),
            };
        }
    }
    return view_port;
}

}  // namespace hitagi::asset