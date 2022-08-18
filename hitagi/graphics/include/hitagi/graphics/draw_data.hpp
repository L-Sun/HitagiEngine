#pragma once
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/texture.hpp>

namespace hitagi::graphics {
struct GuiDrawData {
    math::mat4f                        projection;
    math::vec4u                        view_port;
    resource::Mesh                     mesh;
    std::shared_ptr<resource::Texture> texture;
    std::pmr::vector<math::vec4u>      scissor_rects;
};

struct DebugDrawData {
    math::mat4f    project_view;
    math::vec4u    view_port;
    resource::Mesh mesh;

    struct DebugConstant {
        DebugConstant(math::mat4f transform, math::vec4f color) : transform{transform}, color{color} {}

        math::mat4f transform;
        math::vec4f color;
    };

    std::pmr::vector<DebugConstant> constants;
};

}  // namespace hitagi::graphics