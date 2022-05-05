#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

namespace hitagi::resource {
class Renderable : public SceneObject {
public:
    struct Part {
        std::shared_ptr<Mesh>             mesh;
        std::shared_ptr<MaterialInstance> material;
    };

private:
    std::vector<Part> m_Parts;
    bool              visibility = true;
    // TODO add aabb box, bone, etc.
};
}  // namespace hitagi::resource