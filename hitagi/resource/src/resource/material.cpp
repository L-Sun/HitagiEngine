#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <stdexcept>

namespace hitagi::resource {

auto Material::Builder::Type(MaterialType type) noexcept -> Builder& {
    material_type = type;
    return *this;
}

auto Material::Builder::AppendParameterImpl(std::string_view name, const std::type_info& type_id, std::size_t size) -> Builder& {
    std::pmr::string param_name{name};
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
    std::pmr::string param_name{name};
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
    auto result = Material::Create(*this);
    result->InitDefaultMaterialInstance();
    return result;
}

Material::Material(const Builder& builder)
    : m_Type(builder.material_type),
      m_ParametersInfo(builder.parameters_info),
      m_ValidTextures(builder.texture_name) {
}

auto Material::CreateInstance() const noexcept -> std::shared_ptr<MaterialInstance> {
    return std::make_shared<MaterialInstance>(*m_DefaultInstance);
}

std::size_t Material::GetNumInstances() const noexcept {
    return m_NumInstances;
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

std::size_t Material::GetParametersSize() const noexcept {
    return m_ParametersInfo.back().size + m_ParametersInfo.back().offset;
}

void Material::InitDefaultMaterialInstance() {
    m_DefaultInstance = std::make_shared<MaterialInstance>(shared_from_this());
    m_DefaultInstance->SetName(magic_enum::enum_name(m_Type));

    const auto& end_parameter_info  = m_ParametersInfo.back();
    m_DefaultInstance->m_Parameters = core::Buffer(end_parameter_info.offset + end_parameter_info.size);

    for (const auto& texture : m_ValidTextures) {
        m_DefaultInstance->m_Textures.emplace(texture, nullptr);
    }
}

}  // namespace hitagi::resource