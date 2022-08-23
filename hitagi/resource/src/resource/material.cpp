#include <hitagi/resource/material.hpp>

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

auto Material::Builder::SetWireFrame(bool enable) -> Builder& {
    wireframe = enable;
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
    default_texture_paths.emplace(name, std::move(default_path));
    return *this;
}

void Material::Builder::AddName(const std::pmr::string& name) {
    if (exsisted_names.contains(name)) {
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
}

std::shared_ptr<MaterialInstance> Material::CreateInstance() const noexcept {
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
    m_DefaultInstance = std::make_unique<MaterialInstance>(shared_from_this());

    if (!builder.default_buffer.empty()) {
        m_DefaultInstance->m_Parameters = core::Buffer(builder.default_buffer.size(), builder.default_buffer.data());
    }

    for (const auto& [name, path] : builder.default_texture_paths) {
        auto texture  = std::make_shared<Texture>();
        texture->name = name;
        texture->path = path;
        m_DefaultInstance->m_Textures.emplace(name, texture);
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
    : m_Parameters(other.m_Parameters),
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

std::shared_ptr<Texture> MaterialInstance::GetTexture(std::string_view name) const noexcept {
    std::pmr::string _name(name);
    if (!m_Textures.contains(_name)) return nullptr;
    return m_Textures.at(_name);
}

void MaterialInstance::Warn(std::string_view message) const {
    auto logger = spdlog::get("AssetManager");
    if (logger) logger->warn(message);
}

}  // namespace hitagi::resource