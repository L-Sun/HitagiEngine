#pragma once
#include <hitagi/resource/shader.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>
#include <hitagi/utils/private_build.hpp>

#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <bitset>
#include <variant>

namespace hitagi::resource {
enum struct PrimitiveType : std::uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    LineListAdjacency,
    LineStripAdjacency,
    TriangleListAdjacency,
    TriangleStripAdjacency,
    Unkown,
};

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
    math::mat4f,
    math::vec4f,
    std::shared_ptr<Texture>>;

template <typename T>
concept MaterialParametric = requires(const MaterialParameterValue& parameter) {
    { std::get<T>(parameter) } -> std::same_as<const T&>;
};

struct MaterialParameter {
    std::pmr::string       name;
    MaterialParameterValue value;
};

struct MaterialDetial {
    std::pmr::string                    name;
    PrimitiveType                       primitive = PrimitiveType::TriangleList;
    std::shared_ptr<Shader>             vertex_shader;
    std::shared_ptr<Shader>             pixel_shader;
    std::pmr::vector<MaterialParameter> parameters;
    bool                                wireframe = false;
};

class Material : public utils::enable_private_make_shared_build<Material>,
                 public std::enable_shared_from_this<Material>,
                 private MaterialDetial {
    friend class MaterialInstance;

public:
    class Builder : private MaterialDetial {
        friend class Material;

    public:
        Builder& SetName(std::string_view name);
        Builder& SetPrimitive(PrimitiveType primitive);
        Builder& SetVertexShader(const std::filesystem::path& hlsl_path);
        Builder& SetPixelShader(const std::filesystem::path& hlsl_path);
        Builder& SetWireFrame(bool enable);

        // Set rasizer state
        // Builder& Cull();
        // ...

        template <MaterialParametric T>
        Builder& AppendParameterInfo(std::string_view name, T default_value = {});

        std::shared_ptr<Material> Build();

    private:
        std::pmr::unordered_set<std::pmr::string> exsisted_names;
    };

    std::shared_ptr<MaterialInstance> CreateInstance() noexcept;
    inline const std::size_t          GetNumInstances() const noexcept { return m_NumInstances; }

    inline std::string_view GetName() const noexcept { return name; }
    inline PrimitiveType    GetPrimitiveType() const noexcept { return primitive; }
    inline auto             GetVertexShader() const noexcept { return vertex_shader; }
    inline auto             GetPixelShader() const noexcept { return pixel_shader; }
    inline bool             WireframeEnabled() const noexcept { return wireframe; }

    template <MaterialParametric T>
    bool IsValidParameter(std::string_view name) const noexcept;

    // Get total parameters size
    const std::size_t GetParametersBufferSize() const noexcept;

    bool operator==(const Material& rhs) const;

protected:
    friend class Builder;
    Material(const Builder&);

private:
    std::size_t m_NumInstances = 0;
};

class MaterialInstance {
    friend class Material;

public:
    MaterialInstance(const std::shared_ptr<Material>& material);

    MaterialInstance(const MaterialInstance& other);
    MaterialInstance(MaterialInstance&&) = default;

    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance& operator=(MaterialInstance&&) = default;

    ~MaterialInstance();

    template <MaterialParametric T>
    bool SetParameter(std::string_view name, T value) noexcept;

    template <MaterialParametric T>
    std::optional<T> GetParameter(std::string_view name) const noexcept;

    inline auto GetMaterial() const noexcept { return m_Material; }

    core::Buffer GetParameterBuffer() const;

private:
    std::shared_ptr<Material>           m_Material;
    std::pmr::vector<MaterialParameter> m_Parameters;
};

template <MaterialParametric T>
auto Material::Builder::AppendParameterInfo(std::string_view name, T default_value) -> Builder& {
    parameters.emplace_back(MaterialParameter{.name = std::pmr::string(name), .value = default_value});
    exsisted_names.emplace(std::pmr::string(name));
    return *this;
}

template <MaterialParametric T>
bool Material::IsValidParameter(std::string_view name) const noexcept {
    if (auto iter = std::find_if(parameters.begin(), parameters.end(), [&](const auto& parameter) { return parameter.name == name; }); iter != parameters.end()) {
        return std::holds_alternative<T>(iter->value);
    }
    return false;
}

template <MaterialParametric T>
bool MaterialInstance::SetParameter(std::string_view name, T value) noexcept {
    if (auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& parameter) { return parameter.name == name; }); iter != m_Parameters.end()) {
        if (std::holds_alternative<T>(iter->value)) {
            iter->value = std::move(value);
            return true;
        }
    }
    return false;
}

template <MaterialParametric T>
std::optional<T> MaterialInstance::GetParameter(std::string_view name) const noexcept {
    if (auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(), [&](const auto& parameter) { return parameter.name == name; }); iter != m_Parameters.end()) {
        if (std::holds_alternative<T>(iter->value)) {
            return std::get<T>(iter->value);
        }
    }
    return std::nullopt;
}

}  // namespace hitagi::resource