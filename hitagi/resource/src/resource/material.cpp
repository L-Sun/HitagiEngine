#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <stdexcept>

namespace hitagi::resource {

auto Material::Builder::SetName(std::string_view _name) -> Builder& {
    name = _name;
    return *this;
}

auto Material::Builder::SetPrimitive(PrimitiveType type) -> Builder& {
    primitive = type;
    return *this;
}

auto Material::Builder::SetVertexShader(const std::filesystem::path& path) -> Builder& {
    vertex_shader = std::make_shared<Shader>(Shader::Type::Vertex, path);
    return *this;
}
auto Material::Builder::SetPixelShader(const std::filesystem::path& path) -> Builder& {
    pixel_shader = std::make_shared<Shader>(Shader::Type::Pixel, path);
    return *this;
}

auto Material::Builder::AppendParameterImpl(std::string_view name, const std::type_info& type_id, std::size_t size, std::byte* default_values) -> Builder& {
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

    default_buffer.resize(offset + size);
    if (default_values)
        std::copy_n(default_values, size, default_buffer.data() + offset);

    return *this;
}

auto Material::Builder::AppendTextureName(std::string_view name, std::filesystem::path default_path) -> Builder& {
    std::pmr::string param_name{name};
    AddName(param_name);
    texture_name.emplace(name);
    default_textures.emplace(name, std::move(default_path));
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
    result->InitDefaultMaterialInstance(*this);
    return result;
}

Material::Material(const Builder& builder)
    : MaterialDetial{builder} {
    SetName(builder.name);
}

auto Material::CreateInstance() const noexcept -> std::shared_ptr<MaterialInstance> {
    // we increase the num instances in the constructor of MaterialInstance
    return std::make_shared<MaterialInstance>(*m_DefaultInstance);
}

std::size_t Material::GetNumInstances() const noexcept {
    // remove the default instance
    return m_NumInstances - 1;
}

bool Material::IsValidTextureParameter(std::string_view name) const noexcept {
    return texture_name.count(std::pmr::string(name));
}

auto Material::GetParameterInfo(std::string_view name) const noexcept -> std::optional<ParameterInfo> {
    auto iter = std::find_if(
        std::begin(parameters_info),
        std::end(parameters_info),
        [name](const ParameterInfo& info) {
            return info.name == name;
        });

    if (iter != std::end(parameters_info)) return *iter;
    return std::nullopt;
}

std::size_t Material::GetParametersSize() const noexcept {
    if (parameters_info.empty()) return 0;
    return parameters_info.back().size + parameters_info.back().offset;
}

void Material::InitDefaultMaterialInstance(const Builder& builder) {
    m_DefaultInstance = std::make_shared<MaterialInstance>(shared_from_this());
    m_DefaultInstance->SetName(fmt::format("{}-{}", builder.name, m_NumInstances));

    if (!builder.default_buffer.empty()) {
        m_DefaultInstance->m_Parameters = core::Buffer(builder.default_buffer.data(), builder.default_buffer.size());
    }

    for (const auto& texture : texture_name) {
        m_DefaultInstance->m_Textures.emplace(texture, std::make_shared<Texture>(builder.default_textures.at(texture)));
    }
}

bool Material::operator==(const Material& rhs) const {
    if (parameters_info.size() != rhs.parameters_info.size()) return false;

    for (std::size_t i = 0; i < parameters_info.size(); i++) {
        bool result = parameters_info[i].name == rhs.parameters_info[i].name &&
                      parameters_info[i].size == rhs.parameters_info[i].size &&
                      parameters_info[i].offset == rhs.parameters_info[i].offset &&
                      parameters_info[i].type == rhs.parameters_info[i].type;
        if (result == false) return false;
    }

    return vertex_shader == rhs.vertex_shader &&
           pixel_shader == rhs.pixel_shader &&
           texture_name == rhs.texture_name;
}

}  // namespace hitagi::resource