#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/gfx/common_types.hpp>

namespace hitagi::asset {
class Camera : public Resource {
    friend class CameraNode;

public:
    struct Parameters {
        float aspect    = 16.0f / 9.0f;
        float near_clip = 1.0f;
        float far_clip  = 1000.0f;
        float fov       = 60.0_deg;

        math::vec3f eye      = {0.0f, 0.0f, 0.0f};
        math::vec3f look_dir = {0.0f, 0.0f, 1.0f};
        math::vec3f up       = {0.0f, 1.0f, 0.0f};
    } parameters;

    Camera(Parameters parameters, std::string_view name = "", xg::Guid guid = {})
        : Resource(name, guid), parameters(parameters) {}

    auto GetViewPort(std::uint32_t screen_width, std::uint32_t screen_height) const noexcept -> gfx::ViewPort;
};

}  // namespace hitagi::asset