#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/transform.hpp>

namespace hitagi::resource {

class Geometry : public SceneObject {
public:
    Geometry(std::shared_ptr<Transform> transform);
    void SetTransform(std::shared_ptr<Transform> transform) noexcept;

    bool IsVisiable() const noexcept;
    void SetVisibility(bool visibility) noexcept;

    std::pmr::vector<Mesh>     meshes;
    std::shared_ptr<Transform> transform;
    bool                       visiable;
};

}  // namespace hitagi::resource