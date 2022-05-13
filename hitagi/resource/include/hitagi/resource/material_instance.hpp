#pragma once

#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/material.hpp>

#include <spdlog/spdlog.h>

#include <optional>

namespace hitagi::resource {

class MaterialInstance : public SceneObject {
    friend class Material;

public:
    MaterialInstance(allocator_type alloc = {}) : SceneObject(alloc), m_Parameters(alloc), m_Textures(alloc) {}
    MaterialInstance(const MaterialInstance& other, allocator_type alloc = {});
    MaterialInstance(MaterialInstance&&) noexcept = default;

    MaterialInstance& operator=(const MaterialInstance&)     = default;
    MaterialInstance& operator=(MaterialInstance&&) noexcept = default;

    template <MaterialParametric T>
    MaterialInstance& SetParameter(std::string_view name, const T& value) noexcept;

    template <MaterialParametric T, std::size_t N>
    MaterialInstance& SetParameter(std::string_view name, const std::array<T, N> value) noexcept;

    // TODO set a sampler
    MaterialInstance& SetTexture(std::string_view name, std::shared_ptr<Texture> texture) noexcept;

    template <MaterialParametric T>
    std::optional<T> GetValue(std::string_view name) const noexcept;

    template <MaterialParametric T, std::size_t N>
    std::optional<std::array<T, N>> GetValue(std::string_view name) const noexcept;

    std::shared_ptr<Texture> GetTexture(std::string_view name) const noexcept;

    inline auto GetMaterial() const noexcept { return m_Material; }

private:
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
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->warn("You are setting a invalid parameter: {}", name);
    }

    return *this;
}

template <MaterialParametric T, std::size_t N>
MaterialInstance& MaterialInstance::SetParameter(std::string_view name, std::array<T, N> value) noexcept {
    auto material = m_Material.lock();

    if (material && material->IsValidParameter<decltype(value)>(name)) {
        std::copy(value.begin(), value.end(), &GetParameter<T>(name));
    } else {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->warn("You are setting a invalid parameter: {}", name);
    }

    return *this;
}

template <MaterialParametric T>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<T> {
    auto material = m_Material.lock();
    if (material && material->IsValidParameter<T>(name)) {
        return GetParameter<T>(name);
    } else {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->warn("You are setting a invalid parameter: {}", name);
    }

    return std::nullopt;
}

template <MaterialParametric T, std::size_t N>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<std::array<T, N>> {
    auto material = m_Material.lock();

    if (material && material->IsValidParameter<std::array<T, N>>(name)) {
        std::array<T, N> result{};
        std::copy_n(&GetParameter<T>(name), N, result.data());
        return result;
    } else {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->warn("You are setting a invalid parameter: {}", name);
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