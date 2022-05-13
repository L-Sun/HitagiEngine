#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

#include <spdlog/logger.h>
#include <magic_enum.hpp>

#include <stdexcept>

namespace hitagi::resource {

Material::Builder::Builder(allocator_type alloc) : allocator(alloc), parameters_info(alloc), texture_name(alloc) {}

auto Material::Builder::Type(MaterialType type) noexcept -> Builder& {
    material_type = type;
    return *this;
}

auto Material::Builder::AppendParameterImpl(std::string_view name, const std::type_info& type_id, std::size_t size) -> Builder& {
    std::pmr::string param_name{name, allocator};
    AddName(param_name);

    std::size_t offset = 0;
    if (!parameters_info.empty()) {
        auto& last_parameter        = parameters_info.back();
        offset                      = last_parameter.offset + last_parameter.size;
        const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;

        // Variables are packed into a given four-component vector until the variable will straddle a 4-vector boundary
        offset += remaining >= size ? 0 : remaining;
    }
    parameters_info.emplace_back(ParameterInfo{
        .name   = param_name,
        .type   = std::type_index(type_id),
        .offset = offset,
        .size   = size});

    return *this;
}

auto Material::Builder::AppendTextureName(std::string_view name) -> Builder& {
    std::pmr::string param_name{name, allocator};
    AddName(param_name);

    texture_name.emplace(name);
    return *this;
}

void Material::Builder::AddName(const std::pmr::string& name) {
    if (exsisted_names.count(name) != 0) {
        auto logger = spdlog::get("AssetManager");
        if (logger) logger->error("Duplicated material parameter name: ({})", name);
        throw std::invalid_argument(fmt::format("Duplicated material parameter name: ({})", name));
    }
    exsisted_names.emplace(name);
}

std::shared_ptr<Material> Material::Builder::Build() {
    auto result = Material::Create(*this, allocator);
    result->InitDefaultMaterialInstance();
    return result;
}

Material::Material(const Builder& builder, allocator_type alloc)
    : SceneObject(alloc),
      m_Allocator(alloc),
      m_Type(builder.material_type),
      m_ParametersInfo(builder.parameters_info, alloc),
      m_ValidTextures(builder.texture_name, alloc),
      m_DefaultInstance(std::allocate_shared<MaterialInstance>(alloc)) {
}

auto Material::CreateInstance() const noexcept -> std::shared_ptr<MaterialInstance> {
    return std::allocate_shared<MaterialInstance>(m_Allocator, *m_DefaultInstance);
}

bool Material::IsValidTextureParameter(std::string_view name) const noexcept {
    return m_ValidTextures.count(std::pmr::string(name));
}

auto Material::GetParameterInfo(std::string_view name) const noexcept -> std::optional<ParameterInfo> {
    auto iter = std::find_if(
        std::begin(m_ParametersInfo),
        std::end(m_ParametersInfo),
        [name](const ParameterInfo& info) {
            return info.name == name;
        });

    if (iter != std::end(m_ParametersInfo)) return *iter;
    return std::nullopt;
}

const MaterialInstance& Material::GetDefaultMaterialInstance() const noexcept {
    return *m_DefaultInstance.get();
}

void Material::InitDefaultMaterialInstance() {
    m_DefaultInstance->SetName(magic_enum::enum_name(m_Type));
    m_DefaultInstance->m_Material = shared_from_this();

    const auto& end_parameter_info  = m_ParametersInfo.back();
    m_DefaultInstance->m_Parameters = core::Buffer(end_parameter_info.offset + end_parameter_info.size);

    for (const auto& texture : m_ValidTextures) {
        m_DefaultInstance->m_Textures.emplace(texture, nullptr);
    }
}

}  // namespace hitagi::resource