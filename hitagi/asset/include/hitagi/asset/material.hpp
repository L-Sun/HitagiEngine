#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/texture.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>
#include <hitagi/gfx/device.hpp>

#include <variant>
#include <set>

namespace hitagi::asset {

using MaterialParameterValue = std::variant<
    float,
    std::int32_t,
    std::uint32_t,
    math::vec2i,
    math::vec2u,
    math::vec2f,
    math::vec3i,
    math::vec3u,
    math::vec3f,
    math::vec4i,
    math::vec4u,
    math::vec4f,
    math::mat4f,
    std::shared_ptr<Texture>>;

template <typename T>
concept MaterialParametric = requires(const MaterialParameterValue& parameter) {
    { std::get<T>(parameter) } -> std::same_as<const T&>;
};

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;
};

class MaterialInstance;

struct MaterialDesc {
    std::pmr::vector<gfx::ShaderDesc>   shaders;
    gfx::RenderPipelineDesc             pipeline;
    std::pmr::vector<MaterialParameter> parameters;
};

class Material : public Resource, public std::enable_shared_from_this<Material> {
public:
    Material(MaterialDesc desc, std::string_view name = "");

    Material(const Material&)            = delete;
    Material(Material&&)                 = delete;
    Material& operator=(const Material&) = delete;
    Material& operator=(Material&&)      = delete;

    inline const auto& GetInstances() const noexcept { return m_Instances; }
    inline const auto& GetDefaultParameters() const noexcept { return m_Desc.parameters; }
    inline const auto& GetPipeline() const noexcept { return m_Pipeline; }
    inline const auto& GetMaterialBuffer() const noexcept { return m_MaterialConstantBuffer; }

    auto CalculateMaterialBufferSize() const noexcept -> std::size_t;
    auto CreateInstance() -> std::shared_ptr<MaterialInstance>;

    void InitPipeline(gfx::Device& device);

protected:
    void AddInstance(MaterialInstance* instance) noexcept;
    void RemoveInstance(MaterialInstance* instance) noexcept;

    friend class MaterialInstance;

    std::set<MaterialInstance*> m_Instances;

    MaterialDesc m_Desc;

    bool                                           m_Dirty = true;
    std::pmr::vector<std::shared_ptr<gfx::Shader>> m_Shaders;
    std::shared_ptr<gfx::RenderPipeline>           m_Pipeline               = nullptr;
    std::shared_ptr<gfx::GPUBuffer>                m_MaterialConstantBuffer = nullptr;
};

class MaterialInstance : public Resource {
public:
    MaterialInstance(std::pmr::vector<MaterialParameter> parameters = {}, std::string_view name = "");
    MaterialInstance(const MaterialInstance&);
    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance(MaterialInstance&&) = default;
    MaterialInstance& operator=(MaterialInstance&&) noexcept;
    ~MaterialInstance();

    void        SetMaterial(std::shared_ptr<Material> material);
    inline auto GetMaterial() const noexcept { return m_Material; }

    template <MaterialParametric T>
    void SetParameter(std::string_view name, T value) noexcept;
    template <MaterialParametric T>
    auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;
    auto GetMateriaBufferData() const noexcept -> core::Buffer;
    auto GetTextures() const noexcept -> std::pmr::vector<std::shared_ptr<Texture>>;

private:
    std::shared_ptr<Material>           m_Material = nullptr;
    std::pmr::vector<MaterialParameter> m_Parameters;
};

template <MaterialParametric T>
void MaterialInstance::SetParameter(std::string_view name, T value) noexcept {
    if (auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [name](const auto& param) {
            return param.name == name && std::holds_alternative<T>(param.value);
        });
        iter != m_Parameters.end()) {
        std::get<T>(iter->value) = value;
    } else {
        m_Parameters.emplace_back(MaterialParameter{
            .name  = std::pmr::string{name},
            .value = value,
        });
    }
}

template <MaterialParametric T>
auto MaterialInstance::GetParameter(std::string_view name) const noexcept -> std::optional<T> {
    if (auto iter = std::find_if(m_Parameters.cbegin(), m_Parameters.cend(), [name](const auto& param) {
            return param.name == name && std::holds_alternative<T>(param.value);
        });
        iter != m_Parameters.end()) {
        return std::get<T>(iter->value);
    }
    return std::nullopt;
}
}  // namespace hitagi::asset