#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
Scene::Scene(SupportedMaterails materials, allocator_type alloc)
    : SceneObject(alloc),
      m_Geometries(alloc),
      m_Lights(alloc),
      m_Materials(std::move(materials)) {}

std::shared_ptr<Material> Scene::GetMaterial(MaterialType type) const {
    return m_Materials.at(magic_enum::enum_index(type).value());
}

}  // namespace hitagi::resource