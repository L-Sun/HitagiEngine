#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/texture.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>
#include <hitagi/gfx/device.hpp>

#include <variant>
#include <unordered_set>
#include <array>

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

class MaterialInstance;

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;

    inline bool operator==(const MaterialParameter& rhs) const noexcept { return name == rhs.name && value == rhs.value; }
};

using MaterialParameters = std::pmr::vector<MaterialParameter>;

struct MaterialDesc {
    std::pmr::vector<gfx::ShaderDesc> shaders;
    gfx::RenderPipelineDesc           pipeline;
    MaterialParameters                parameters;
};

class Material : public Resource, public std::enable_shared_from_this<Material> {
public:
    Material(MaterialDesc desc, std::string_view name = "");

    Material(const Material&)            = delete;
    Material(Material&&)                 = delete;
    Material& operator=(const Material&) = delete;
    Material& operator=(Material&&)      = delete;

    auto               CreateInstance() -> std::shared_ptr<MaterialInstance>;
    inline const auto& GetInstances() const noexcept { return m_Instances; }
    inline const auto& GetDefaultParameters() const noexcept { return m_Desc.parameters; }
    auto               CalculateMaterialBufferSize() const noexcept -> std::size_t;
    auto               GetPipeline(gfx::Device& device) -> std::shared_ptr<gfx::RenderPipeline>;

    template <MaterialParametric>
    bool HasParameter(std::string_view name) const noexcept;

protected:
    friend MaterialInstance;

    void AddInstance(MaterialInstance* instance) noexcept;
    void RemoveInstance(MaterialInstance* instance) noexcept;

    std::pmr::unordered_set<MaterialInstance*> m_Instances;

    MaterialDesc                                   m_Desc;
    std::pmr::vector<std::shared_ptr<gfx::Shader>> m_Shaders;
    std::shared_ptr<gfx::RenderPipeline>           m_Pipeline = nullptr;
};

struct SplitMaterialParameters {
    MaterialParameters only_in_instance;
    MaterialParameters only_in_material;
    MaterialParameters in_both;
};

class MaterialInstance : public Resource {
public:
    MaterialInstance(MaterialParameters parameters = {}, std::string_view name = "");
    MaterialInstance(const MaterialInstance&);
    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance(MaterialInstance&&) noexcept = default;
    MaterialInstance& operator=(MaterialInstance&&) noexcept;
    ~MaterialInstance();

    void        SetMaterial(std::shared_ptr<Material> material);
    inline auto GetMaterial() const noexcept { return m_Material; }

    inline const auto& GetParameters() const noexcept { return m_Parameters; }
    inline auto&       GetParameters() noexcept { return m_Parameters; }

    void SetParameter(MaterialParameter parameter) noexcept;
    template <MaterialParametric T>
    void SetParameter(std::string_view name, T value) noexcept;
    template <MaterialParametric T>
    auto GetParameter(std::string_view name) const noexcept -> std::optional<T>;
    auto GetSplitParameters() const noexcept -> SplitMaterialParameters;
    auto GetAssociatedTextures() const noexcept -> std::pmr::vector<std::shared_ptr<Texture>>;
    auto GenerateMaterialBuffer() const noexcept -> core::Buffer;

    template <MaterialParametric T>
    bool HasParameter(std::string_view name) const noexcept;

private:
    std::shared_ptr<Material> m_Material = nullptr;
    MaterialParameters        m_Parameters;
};

template <MaterialParametric T>
bool Material::HasParameter(std::string_view name) const noexcept {
    for (const auto& param : m_Desc.parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return true;
        }
    }
    return false;
}

template <MaterialParametric T>
void MaterialInstance::SetParameter(std::string_view name, T value) noexcept {
    SetParameter(MaterialParameter{.name = std::pmr::string(name), .value = value});
}

template <MaterialParametric T>
auto MaterialInstance::GetParameter(std::string_view name) const noexcept -> std::optional<T> {
    for (const auto& param : m_Parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return std::get<T>(param.value);
        }
    }
    return std::nullopt;
}

template <MaterialParametric T>
bool MaterialInstance::HasParameter(std::string_view name) const noexcept {
    for (const auto& param : m_Parameters) {
        if (param.name == name && std::holds_alternative<T>(param.value)) {
            return true;
        }
    }
    return false;
}

}  // namespace hitagi::asset