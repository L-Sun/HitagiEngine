#include <hitagi/asset/material.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include <stdexcept>
#include <variant>

namespace hitagi::asset {

Material::Material(gfx::RenderPipeline::Desc pipeline_desc, std::pmr::vector<Parameter> parameters, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_PipelineDesc(std::move(pipeline_desc)),
      m_DefaultParameters(std::move(parameters)) {
    auto iter = std::unique(m_DefaultParameters.rbegin(), m_DefaultParameters.rend(), [](const auto& lhs, const auto& rhs) {
        return lhs.name == rhs.name;
    });
    m_DefaultParameters.erase(m_DefaultParameters.rend().base(), iter.base());
}

auto Material::Create(gfx::RenderPipeline::Desc pipeline_desc, std::pmr::vector<Parameter> parameters, std::string_view name, xg::Guid guid) -> std::shared_ptr<Material> {
    struct CreateTemp : public Material {
        CreateTemp(gfx::RenderPipeline::Desc pipeline_desc, std::pmr::vector<Parameter> parameters, std::string_view name, xg::Guid guid)
            : Material(std::move(pipeline_desc), std::move(parameters), name, guid) {}
    };
    return std::make_shared<CreateTemp>(std::move(pipeline_desc), std::move(parameters), name, guid);
}

auto Material::CalculateMaterialBufferSize() const noexcept -> std::size_t {
    std::size_t offset = 0;
    for (const auto& parameter : m_DefaultParameters) {
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

auto Material::CreateInstance() -> std::shared_ptr<MaterialInstance> {
    auto instance_name = fmt::format("{}-{}", m_Name, m_NumInstance);
    auto result        = std::make_shared<MaterialInstance>(m_DefaultParameters, instance_name);
    result->SetMaterial(shared_from_this());
    return result;
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

    std::size_t        offset        = 0;
    std::int32_t       texture_index = 0;
    const std::int32_t no_texture    = -1;
    for (const auto& default_param : m_Material->m_DefaultParameters) {
        if (std::holds_alternative<std::shared_ptr<Texture>>(default_param.value)) {
            const std::size_t remaining = (~(offset & 0xf) & 0xf) + 0x1;
            offset += remaining >= sizeof(texture_index) ? 0 : remaining;

            auto default_tex = std::get<std::shared_ptr<Texture>>(default_param.value);
            auto tex         = GetParameter<std::shared_ptr<Texture>>(default_param.name);

            if (tex.has_value()) {
                (*reinterpret_cast<std::int32_t*>(result.GetData() + offset)) = tex.value() != nullptr ? texture_index++ : no_texture;
            } else {
                (*reinterpret_cast<std::int32_t*>(result.GetData() + offset)) = default_tex != nullptr ? texture_index++ : no_texture;
            }

            offset += sizeof(texture_index);
        } else {
            std::visit([&](auto&& default_value) {
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