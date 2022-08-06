#pragma once
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/transform.hpp>

namespace hitagi::resource {
struct PipelineParameters {
    std::optional<math::vec4u> view_port;
    std::optional<math::vec4u> scissor_react;
};

struct Renderable {
    enum struct Type : std::uint8_t {
        Default,
        UI,
        Debug,
    };

    Type         type     = Type::Default;
    VertexArray* vertices = nullptr;
    IndexArray*  indices  = nullptr;

    std::size_t index_count   = 0;
    std::size_t vertex_offset = 0;
    std::size_t index_offset  = 0;

    Material*               material          = nullptr;
    const MaterialInstance* material_instance = nullptr;

    math::mat4f transform;

    PipelineParameters pipeline_parameters;

    // Use by graphics manager
    std::size_t object_constant_offset   = 0;
    std::size_t material_constant_offset = 0;
};

}  // namespace hitagi::resource