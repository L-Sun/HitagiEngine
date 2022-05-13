#pragma once
#include <hitagi/resource/geometry.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>

#include <magic_enum.hpp>

namespace hitagi::resource {
class Scene : public SceneObject {
public:
    using allocator_type     = std::pmr::polymorphic_allocator<>;
    using SupportedMaterails = std::array<std::shared_ptr<Material>, magic_enum::enum_count<MaterialType>()>;

    Scene(SupportedMaterails materials, allocator_type alloc = {});
    void AddGeometry(Geometry geometry);
    void AddCamera(std::shared_ptr<Camera> camera);
    void AddLight(std::shared_ptr<Light> light);

    std::shared_ptr<Material> GetMaterial(MaterialType type) const;

    std::pmr::vector<Mesh> GetRenderable();

private:
    std::pmr::vector<Geometry>                m_Geometries;
    std::pmr::vector<std::shared_ptr<Light>>  m_Lights;
    std::pmr::vector<std::shared_ptr<Camera>> m_Camera;

    SupportedMaterails m_Materials;
};

}  // namespace hitagi::resource