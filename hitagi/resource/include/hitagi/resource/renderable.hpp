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

    Type                                   type = Type::Default;
    std::shared_ptr<resource::VertexArray> vertices;
    std::shared_ptr<resource::IndexArray>  indices;
    resource::Mesh::SubMesh                sub_mesh;
    resource::Transform                    transform;
    std::shared_ptr<resource::Material>    material = nullptr;

    PipelineParameters pipeline_parameters;

    // Use by graphics manager
    std::size_t object_constant_offset   = 0;
    std::size_t material_constant_offset = 0;
};

}  // namespace hitagi::resource