#include <hitagi/asset/material.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <variant>

namespace hitagi::asset {

Material::Material(std::pmr::vector<gfx::ShaderDesc> shader_desc,
                   gfx::RenderPipelineDesc           pipeline_desc,
                   std::pmr::vector<Parameter>       parameters,
                   std::string_view                  name,
                   xg::Guid                          guid)
    : Resource(name, guid),
      m_ShaderDesc(std::move(shader_desc)),
      m_PipelineDesc(std::move(pipeline_desc)),
      m_DefaultParameters(std::move(parameters))

{
    m_PipelineDesc.name = m_Name;

    auto iter = std::unique(m_DefaultParameters.rbegin(), m_DefaultParameters.rend(), [](const auto& lhs, const auto& rhs) {
        return lhs.name == rhs.name;
    });
    m_DefaultParameters.erase(m_DefaultParameters.rend().base(), iter.base());
}

auto Material::Create(std::pmr::vector<gfx::ShaderDesc> shader_desc, gfx::RenderPipelineDesc pipeline_desc, std::pmr::vector<Parameter> parameters, std::string_view name, xg::Guid guid) -> std::shared_ptr<Material> {
    struct CreateTemp : public Material {
        CreateTemp(std::pmr::vector<gfx::ShaderDesc> shader_desc, gfx::RenderPipelineDesc pipeline_desc, std::pmr::vector<Parameter> parameters, std::string_view name, xg::Guid guid)
            : Material(std::move(shader_desc), std::move(pipeline_desc), std::move(parameters), name, guid) {}
    };
    return std::make_shared<CreateTemp>(std::move(shader_desc), std::move(pipeline_desc), std::move(parameters), name, guid);
}

auto Material::CalculateMaterialBufferSize() const noexcept -> std::size_t {
    std::size_t offset = 0;
    for (const auto& parameter : m_DefaultParameters) {
        // We use int32_t to indicate its index in texture array
        if (std::holds_alternative<std::shared_ptr<Texture>>(parameter.value)) {
        } else {
            std::visit(
                [&](auto&& value) {
                    const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
                    offset += remaining >= sizeof(value) ? 0 : remaining;

                    offset += sizeof(value);
                },
                parameter.value);
        }
    }
    return utils::align(offset, 16);
}

auto Material::CreateInstance() -> std::shared_ptr<MaterialInstance> {
    auto instance_name = fmt::format("{}-{}", m_Name, m_NumInstance);
    auto result        = std::make_shared<MaterialInstance>(m_DefaultParameters, instance_name);
    result->SetMaterial(shared_from_this());
    return result;
}

void Material::InitPipeline(gfx::Device& device) {
    if (!m_Dirty) return;
    std::transform(m_ShaderDesc.begin(), m_ShaderDesc.end(), std::back_inserter(m_Shaders), [&](const auto& desc) {
        return device.CreateShader(desc);
    });
    std::transform(m_Shaders.begin(), m_Shaders.end(), std::back_inserter(m_PipelineDesc.shaders), [&](const auto& shader) {
        return std::weak_ptr<gfx::Shader>(shader);
    });

    if (auto iter = std::find_if(m_Shaders.begin(), m_Shaders.end(), [](const auto& shader) {
            return shader->GetDesc().type == gfx::ShaderType::Vertex;
        });
        iter != m_Shaders.end()) {
        auto vertex_shader                 = *iter;
        m_PipelineDesc.vertex_input_layout = device.GetShaderCompiler().ExtractVertexLayout(vertex_shader->GetDesc());
    }

    m_Pipeline = device.CreateRenderPipeline(m_PipelineDesc);
    m_Dirty    = false;
}

MaterialInstance::MaterialInstance(std::pmr::vector<Material::Parameter> parameters, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_Parameters(std::move(parameters)) {}

MaterialInstance::MaterialInstance(const MaterialInstance& other) : Resource(other), m_Parameters(other.m_Parameters) {
    SetMaterial(other.GetMaterial());
}

MaterialInstance& MaterialInstance::operator=(const MaterialInstance& rhs) {
    if (this != &rhs) {
        Resource::operator=(rhs);
        m_Parameters = rhs.m_Parameters;
        SetMaterial(rhs.m_Material);
    }
    return *this;
}

MaterialInstance& MaterialInstance::operator=(MaterialInstance&& rhs) noexcept {
    if (this != &rhs) {
        Resource::operator=(std::move(rhs));
        m_Parameters = std::move(rhs.m_Parameters);

        if (m_Material) m_Material->m_NumInstance--;
        m_Material = std::move(rhs.m_Material);
    }
    return *this;
}

MaterialInstance::~MaterialInstance() {
    SetMaterial(nullptr);
}

void MaterialInstance::SetMaterial(std::shared_ptr<Material> material) {
    if (material == m_Material) return;

    if (m_Material != nullptr) {
        m_Material->m_NumInstance--;
    }

    m_Material = std::move(material);

    if (m_Material != nullptr) {
        m_Material->m_NumInstance++;
    }
}

auto MaterialInstance::GetMateriaBufferData() const noexcept -> core::Buffer {
    if (m_Material == nullptr) return {};

    core::Buffer result(m_Material->CalculateMaterialBufferSize());

    std::size_t offset = 0;
    for (const auto& default_param : m_Material->m_DefaultParameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(default_param.value)) {
        } else {
            std::visit(
                [&](auto&& default_value) {
                    const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
                    offset += remaining >= sizeof(default_value) ? 0 : remaining;

                    auto param = GetParameter<std::remove_cvref_t<decltype(default_value)>>(default_param.name);
                    if (param.has_value()) {
                        std::memcpy(result.GetData() + offset, &(param.value()), sizeof(default_value));
                    } else {
                        std::memcpy(result.GetData() + offset, &default_value, sizeof(default_value));
                    }

                    offset += sizeof(default_value);
                },
                default_param.value);
        }
    }

    return result;
}

auto MaterialInstance::GetTextures() const noexcept -> std::pmr::vector<std::shared_ptr<Texture>> {
    std::pmr::vector<std::shared_ptr<Texture>> result;
    for (const auto& default_param : m_Material->m_DefaultParameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(default_param.value)) {
            auto tex = GetParameter<std::shared_ptr<Texture>>(default_param.name);
            if (tex.has_value()) {
                result.emplace_back(tex.value());
            } else {
                result.emplace_back(std::get<std::shared_ptr<Texture>>(default_param.value));
            }
        }
    }
    return result;
}

}  // namespace hitagi::asset