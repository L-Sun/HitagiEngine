#pragma once

#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/material.hpp>

#include <optional>

namespace hitagi::resource {

class MaterialInstance : public SceneObject {
    friend class Material;

public:
    MaterialInstance(const std::shared_ptr<Material>& material);
    MaterialInstance(const MaterialInstance& other);
    MaterialInstance(MaterialInstance&&) = default;

    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance& operator=(MaterialInstance&&) = default;

    ~MaterialInstance();

    template <MaterialParametric T>
    MaterialInstance& SetParameter(std::string_view name, const T& value) noexcept;

    template <MaterialParametric T>
    MaterialInstance& SetParameter(std::string_view name, const std::pmr::vector<T> value) noexcept;

    // TODO set a sampler
    MaterialInstance& SetTexture(std::string_view name, std::shared_ptr<Texture> texture) noexcept;

    MaterialInstance& SetMaterial(const std::shared_ptr<Material>& material) noexcept;

    template <MaterialParametric T>
    std::optional<T> GetValue(std::string_view name) const noexcept;

    template <MaterialParametric T, std::size_t N>
    std::optional<std::array<T, N>> GetValue(std::string_view name) const noexcept;

    std::shared_ptr<Texture> GetTexture(std::string_view name) const noexcept;

    inline auto GetMaterial() const noexcept { return m_Material; }

    inline auto& GetParameterBuffer() const noexcept { return m_Parameters; }

    inline auto& GetTextures() const noexcept { return m_Textures; }

private:
    void Warn(std::string_view message) const;

    template <typename T>
    const T& GetParameter(std::string_view name) const noexcept;

    template <typename T>
    T& GetParameter(std::string_view name) noexcept;

    std::weak_ptr<Material> m_Material;

    core::Buffer                                                        m_Parameters;
    std::pmr::unordered_map<std::pmr::string, std::shared_ptr<Texture>> m_Textures;
};

template <MaterialParametric T>
MaterialInstance& MaterialInstance::SetParameter(std::string_view name, const T& value) noexcept {
    auto material = m_Material.lock();

    if (material && material->IsValidParameter<T>(name)) {
        GetParameter<T>(name) = value;
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return *this;
}

template <MaterialParametric T>
MaterialInstance& MaterialInstance::SetParameter(std::string_view name, std::pmr::vector<T> values) noexcept {
    auto material = m_Material.lock();

    if (material && material->IsValidParameterArray<T>(name, values.size())) {
        std::copy(values.begin(), values.end(), &GetParameter<T>(name));
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return *this;
}

template <MaterialParametric T>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<T> {
    auto material = m_Material.lock();
    if (material && material->IsValidParameter<T>(name)) {
        return GetParameter<T>(name);
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return std::nullopt;
}

template <MaterialParametric T, std::size_t N>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<std::array<T, N>> {
    auto material = m_Material.lock();

    if (material && material->IsValidParameterArray<T>(name, N)) {
        std::array<T, N> result{};
        std::copy_n(&GetParameter<T>(name), N, result.data());
        return result;
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }
    return std::nullopt;
}

template <typename T>
const T& MaterialInstance::GetParameter(std::string_view name) const noexcept {
    auto material = m_Material.lock();
    auto info     = *material->GetParameterInfo(name);
    return *reinterpret_cast<const T*>(m_Parameters.GetData() + info.offset);
}

template <typename T>
T& MaterialInstance::GetParameter(std::string_view name) noexcept {
    return const_cast<T&>(const_cast<const MaterialInstance*>(this)->GetParameter<T>(name));
}

}  // namespace hitagi::resource