#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/transform.hpp>

namespace hitagi::resource {

class Geometry : public SceneObject {
public:
    Geometry(std::shared_ptr<Transform> transform, allocator_type alloc = {});
    void SetTransform(std::shared_ptr<Transform> transform) noexcept;
    void AddMesh(Mesh mesh) noexcept;

    bool IsVisiable() const noexcept;
    void SetVisibility(bool visibility) noexcept;

private:
    std::pmr::vector<Mesh>     m_Meshes;
    std::shared_ptr<Transform> m_Transform;
    bool                       m_Visibility;
};

}  // namespace hitagi::resource