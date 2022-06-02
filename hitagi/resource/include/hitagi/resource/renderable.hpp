#pragma once
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/transform.hpp>

namespace hitagi::graphics {
class Frame;
}

namespace hitagi::resource {
struct PipelineParameters {
    std::optional<math::vec4u> scissor_react;
};

struct Renderable {
public:
    resource::Mesh                       mesh;
    std::shared_ptr<resource::Material>  material;
    std::shared_ptr<resource::Transform> transform;

    PipelineParameters pipeline_parameters;

private:
    friend class graphics::Frame;
    std::uint64_t object_constant_offset;    // used by graphics
    std::uint64_t material_constant_offset;  // used by graphics
};

}  // namespace hitagi::resource