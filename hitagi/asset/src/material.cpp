#include <hitagi/asset/material.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <stdexcept>
#include <variant>

namespace hitagi::asset {

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

std::shared_ptr<Material> Material::Builder::Build() {
    return Material::Create(*this);
}

Material::Material(const Builder& builder)
    : MaterialDetial{builder} {
}

auto Material::CreateInstance(std::string_view name) noexcept -> std::shared_ptr<MaterialInstance> {
    return std::make_shared<MaterialInstance>(shared_from_this(), name);
}

const std::size_t Material::GetParametersBufferSize() const noexcept {
    std::size_t offset = 0;
    for (const auto& parameter : parameters) {
        // We use int32_t to indicate its index in texture array
        if (std::holds_alternative<std::shared_ptr<Texture>>(parameter.value)) {
            const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
            offset += remaining >= sizeof(std::int32_t) ? 0 : remaining;

            offset += sizeof(std::int32_t);
        } else {
            std::visit([&](auto&& value) {
                const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
                offset += remaining >= sizeof(value) ? 0 : remaining;

                offset += sizeof(value);
            },
                       parameter.value);
        }
    }
    return utils::align(offset, 16);
}

bool Material::operator==(const Material& rhs) const {
    if (parameters.size() != rhs.parameters.size()) return false;

    for (std::size_t i = 0; i < parameters.size(); i++) {
        bool result = parameters[i].name == rhs.parameters[i].name &&
                      parameters[i].value.index() == rhs.parameters[i].value.index();

        if (result == false) return false;
    }

    return vertex_shader == rhs.vertex_shader &&
           pixel_shader == rhs.pixel_shader &&
           wireframe == rhs.wireframe;
}

MaterialInstance::MaterialInstance(const std::shared_ptr<Material>& material, std::string_view name)
    : m_Name(name),
      m_Material(material),
      m_Parameters(material->parameters) {
    if (material == nullptr) {
        if (auto logger = spdlog::get("AssetManager"); logger) {
            logger->error("Can not create a material instance without the material info, since the pointer of material is null!");
        }
        throw std::invalid_argument("Can not create a material instance without the material info, since the pointer of material is null!");
    }
    material->m_NumInstances++;
}

MaterialInstance::MaterialInstance(const MaterialInstance& other)
    : m_Parameters(other.m_Parameters) {
    m_Material->m_NumInstances--;
    m_Material = other.GetMaterial();
    m_Material->m_NumInstances++;
}

MaterialInstance& MaterialInstance::operator=(const MaterialInstance& rhs) {
    if (this != &rhs) {
        m_Material->m_NumInstances--;
        m_Material = rhs.GetMaterial();
        m_Material->m_NumInstances++;
        m_Parameters = rhs.m_Parameters;
    }
    return *this;
}

MaterialInstance::~MaterialInstance() {
    m_Material->m_NumInstances--;
}

core::Buffer MaterialInstance::GetParameterBuffer() const {
    core::Buffer result(m_Material->GetParametersBufferSize());

    std::size_t  offset        = 0;
    std::int32_t texture_index = 0;
    std::int32_t no_texture    = -1;
    for (const auto& parameter : m_Parameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(parameter.value)) {
            const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
            offset += remaining >= sizeof(texture_index) ? 0 : remaining;

            (*reinterpret_cast<std::int32_t*>(result.GetData() + offset)) = std::get<std::shared_ptr<Texture>>(parameter.value) ? texture_index++ : no_texture;

            offset += sizeof(texture_index);
        } else {
            std::visit([&](auto&& value) {
                const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
                offset += remaining >= sizeof(value) ? 0 : remaining;

                std::memcpy(result.GetData() + offset, &value, sizeof(value));

                offset += sizeof(value);
            },
                       parameter.value);
        }
    }

    return result;
}
auto MaterialInstance::GetTextures() const -> std::pmr::vector<std::shared_ptr<Texture>> {
    std::pmr::vector<std::shared_ptr<Texture>> result;
    for (const auto& parameter : m_Parameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(parameter.value)) {
            result.emplace_back(std::get<std::shared_ptr<Texture>>(parameter.value));
        }
    }
    return result;
}

}  // namespace hitagi::asset