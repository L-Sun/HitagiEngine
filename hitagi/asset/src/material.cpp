#include <hitagi/asset/material.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <magic_enum.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/unique.hpp>
#include <spdlog/spdlog.h>

#include <variant>

namespace hitagi::asset {

Material::Material(MaterialDesc desc, std::string_view name)
    : Resource(Type::Material, name), m_Desc(std::move(desc)) {
    if (m_Desc.pipeline.name.empty()) m_Desc.pipeline.name = m_Name;

    m_Desc.parameters = m_Desc.parameters                                                                               //
                        | ranges::views::unique([](const auto& lhs, const auto& rhs) { return lhs.name == rhs.name; })  //
                        | ranges::to<MaterialParameters>();
}

auto Material::CalculateMaterialBufferSize() const noexcept -> std::size_t {
    std::size_t size = 0;
    for (const auto& [name, value] : m_Desc.parameters) {
        size += std::visit(
            utils::Overloaded{
                [](const std::shared_ptr<asset::Texture>&) -> std::size_t { return 0; },
                [](const auto& value) -> std::size_t {
                    return sizeof(value);
                },
            },
            value);
    }
    return size;
}

auto Material::CreateInstance() -> std::shared_ptr<MaterialInstance> {
    const auto instance_name = fmt::format("{}-{}", m_Name, m_Instances.size());
    const auto result        = std::make_shared<MaterialInstance>(m_Desc.parameters, instance_name);
    result->SetMaterial(shared_from_this());
    return result;
}

auto Material::GetPipeline(gfx::Device& device) -> std::shared_ptr<gfx::RenderPipeline> {
    if (!m_Pipeline) {
        m_Shaders = m_Desc.shaders                                                                           //
                    | ranges::views::transform([&](const auto& desc) { return device.CreateShader(desc); })  //
                    | ranges::to<std::pmr::vector<std::shared_ptr<gfx::Shader>>>();

        m_Desc.pipeline.shaders = m_Shaders | ranges::to<std::pmr::vector<std::weak_ptr<gfx::Shader>>>();

        if (auto iter = std::find_if(m_Shaders.begin(), m_Shaders.end(), [](const auto& shader) {
                return shader->GetDesc().type == gfx::ShaderType::Vertex;
            });
            iter != m_Shaders.end()) {
            m_Desc.pipeline.vertex_input_layout = device.GetShaderCompiler().ExtractVertexLayout((*iter)->GetDesc());
        }

        m_Pipeline = device.CreateRenderPipeline(m_Desc.pipeline);
    }
    return m_Pipeline;
}

void Material::AddInstance(MaterialInstance* instance) noexcept {
    m_Instances.emplace(instance);
}

void Material::RemoveInstance(MaterialInstance* instance) noexcept {
    m_Instances.erase(instance);
}

MaterialInstance::MaterialInstance(MaterialParameters parameters, std::string_view name)
    : Resource(Type::MaterialInstance, name),
      m_Parameters(std::move(parameters)) {
    m_Parameters = m_Parameters                                                                                    //
                   | ranges::views::unique([](const auto& lhs, const auto& rhs) { return lhs.name == rhs.name; })  //
                   | ranges::to<MaterialParameters>();
}

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

        if (m_Material) m_Material->RemoveInstance(this);
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
        m_Material->RemoveInstance(this);
    }

    m_Material = std::move(material);

    if (m_Material != nullptr) {
        m_Material->AddInstance(this);
    }
}

void MaterialInstance::SetParameter(MaterialParameter parameter) noexcept {
    const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& param) {
        return param.name == parameter.name;
    });
    if (iter != m_Parameters.end()) {
        *iter = parameter;
    } else {
        m_Parameters.emplace_back(parameter);
    }
}

auto MaterialInstance::GetSplitParameters() const noexcept -> SplitMaterialParameters {
    if (m_Material == nullptr) return SplitMaterialParameters{.only_in_instance = m_Parameters};

    SplitMaterialParameters result;

    for (const auto& [name, default_value] : m_Material->m_Desc.parameters) {
        const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& param) {
            return param.name == name && default_value.index() == param.value.index();
        });
        if (iter != m_Parameters.end()) {
            result.in_both.emplace_back(*iter);
        } else {
            result.only_in_material.emplace_back(MaterialParameter{
                .name  = name,
                .value = default_value,
            });
        }
    }

    for (const auto& param : m_Parameters) {
        const auto iter = std::find_if(m_Material->m_Desc.parameters.begin(), m_Material->m_Desc.parameters.end(), [&](const auto& desc) {
            return desc.name == param.name && desc.value.index() == param.value.index();
        });
        if (iter == m_Material->m_Desc.parameters.end()) {
            result.only_in_instance.emplace_back(param);
        }
    }

    return result;
}

auto MaterialInstance::GetAssociatedTextures() const noexcept -> std::pmr::vector<std::shared_ptr<Texture>> {
    if (m_Material == nullptr) return {};

    std::pmr::vector<std::shared_ptr<Texture>> result;

    for (const auto& [name, default_value] : m_Material->m_Desc.parameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(default_value)) {
            const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& param) {
                return param.name == name && std::holds_alternative<std::shared_ptr<Texture>>(param.value);
            });
            if (iter != m_Parameters.end()) {
                result.emplace_back(std::get<std::shared_ptr<Texture>>(iter->value));
            } else {
                result.emplace_back(std::get<std::shared_ptr<Texture>>(default_value));
            }
        }
    }
    return result;
}

auto MaterialInstance::GenerateMaterialBuffer() const noexcept -> core::Buffer {
    if (m_Material == nullptr) return {};

    core::Buffer result(m_Material->CalculateMaterialBufferSize());

    std::size_t offset = 0;
    for (const auto& [name, default_value] : m_Material->m_Desc.parameters) {
        std::visit(
            utils::Overloaded{
                [&](const std::shared_ptr<Texture>&) {},
                [&](const auto& data) {
                    using T = std::decay_t<decltype(data)>;

                    const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& param) {
                        return param.name == name && std::holds_alternative<T>(param.value);
                    });
                    if (iter != m_Parameters.end()) {
                        auto value = std::get<T>(iter->value);
                        std::memcpy(result.GetData() + offset, &value, sizeof(data));
                    } else {
                        std::memcpy(result.GetData() + offset, &data, sizeof(data));
                    }
                    offset += sizeof(data);
                },
            },
            default_value);
    }

    return result;
}

}  // namespace hitagi::asset