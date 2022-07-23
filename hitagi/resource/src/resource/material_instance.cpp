#include <hitagi/resource/material_instance.hpp>
#include "hitagi/resource/scene_object.hpp"

#include <spdlog/spdlog.h>

namespace hitagi::resource {
MaterialInstance::MaterialInstance(const std::shared_ptr<Material>& material)
    : m_Material(material) {
    if (material == nullptr) {
        if (auto logger = spdlog::get("AssetManager"); logger) {
            logger->error("Can not create a material instance without the material info, since the pointer of material is null!");
        }
        throw std::invalid_argument("Can not create a material instance without the material info, since the pointer of material is null!");
    }
    material->m_NumInstances++;
}

MaterialInstance::MaterialInstance(const MaterialInstance& other)
    : ResourceObject(other),
      m_Parameters(other.m_Parameters),
      m_Textures(other.m_Textures) {
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
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return *this;
}

MaterialInstance& MaterialInstance::SetMaterial(const std::shared_ptr<Material>& material) noexcept {
    auto _m = m_Material.lock();
    if (_m) _m->m_NumInstances--;

    material->m_NumInstances++;

    m_Material = material;
    return *this;
}

std::shared_ptr<Texture> MaterialInstance::GetTexture(std::string_view name) const noexcept {
    return m_Textures.at(std::pmr::string(name));
}

void MaterialInstance::Warn(std::string_view message) const {
    auto logger = spdlog::get("AssetManager");
    if (logger) logger->warn(message);
}

}  // namespace hitagi::resource