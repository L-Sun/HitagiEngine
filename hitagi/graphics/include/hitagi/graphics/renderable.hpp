#pragma once
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/transform.hpp>

namespace hitagi::graphics {
struct PipelineParameters {
    std::optional<math::vec4u> scissor_react;
};

struct Renderable {
public:
    std::shared_ptr<resource::VertexArray>      vertices;
    std::shared_ptr<resource::IndexArray>       indices;
    std::shared_ptr<resource::Material>         material;
    std::shared_ptr<resource::MaterialInstance> material_instance;
    std::shared_ptr<resource::Transform>        transform;
    resource::PrimitiveType                     primitive = resource::PrimitiveType::TriangleList;

    PipelineParameters pipeline_parameters;

private:
    friend class Frame;
    std::uint64_t object_constant_offset;    // used by graphics
    std::uint64_t material_constant_offset;  // used by graphics
};

}  // namespace hitagi::graphics