#include <hitagi/resource/material_instance.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::resource {
MaterialInstance::MaterialInstance(const std::shared_ptr<Material>& material, allocator_type alloc)
    : SceneObject(alloc), m_Material(material), m_Parameters(alloc), m_Textures(alloc) {
    if (material == nullptr) {
        if (auto logger = spdlog::get("AssetManager"); logger) {
            logger->error("Can not create a material instance without the material info, since the pointer of material is null!");
        }
        throw std::invalid_argument("Can not create a material instance without the material info, since the pointer of material is null!");
    }
    material->m_NumInstances++;
}

MaterialInstance::MaterialInstance(const MaterialInstance& other, allocator_type alloc)
    : SceneObject(alloc),
      m_Parameters(other.m_Parameters),
      m_Textures(alloc) {
    if (auto material = m_Material.lock(); material) {
        material->m_NumInstances--;
    }

    m_Material = other.GetMaterial();
    if (auto material = m_Material.lock(); material) {
        material->m_NumInstances++;
    }
}

MaterialInstance& MaterialInstance::operator=(const MaterialInstance& rhs) {
    if (this != &rhs) {
        if (auto material = m_Material.lock(); material) {
            material->m_NumInstances--;
        }

        m_Material = rhs.GetMaterial();
        if (auto material = m_Material.lock(); material) {
            material->m_NumInstances++;
        }

        m_Parameters = rhs.m_Parameters;
        m_Textures   = rhs.m_Textures;
    }
    return *this;
}

MaterialInstance::~MaterialInstance() {
    if (auto material = m_Material.lock(); material) {
        material->m_NumInstances--;
    }
}

MaterialInstance& MaterialInstance::SetTexture(std::string_view name, std::shared_ptr<Texture> texture) noexcept {
    auto material = m_Material.lock();
    if (material && material->IsValidTextureParameter(name)) {
        m_Textures[std::pmr::string(name)] = std::move(texture);
    }

    return *this;
}

}  // namespace hitagi::resource