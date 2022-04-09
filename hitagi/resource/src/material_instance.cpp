#include <hitagi/resource/material_instance.hpp>

namespace hitagi::resource {
MaterialInstance& MaterialInstance::SetTexture(std::string_view name, std::shared_ptr<Texture> texture) noexcept {
    auto material = m_Material.lock();
    if (material && material->IsValidTextureParameter(name)) {
        m_Textures[std::pmr::string(name)] = std::move(texture);
    }

    return *this;
}

}  // namespace hitagi::resource