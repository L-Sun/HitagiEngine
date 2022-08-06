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

template <typename T>
concept MaterialParametric =
    std::same_as<float, T> ||
    std::same_as<std::int32_t, T> ||
    std::same_as<std::uint32_t, T> ||
    std::same_as<math::vec2i, T> ||
    std::same_as<math::vec2u, T> ||
    std::same_as<math::vec2f, T> ||
    std::same_as<math::vec3i, T> ||
    std::same_as<math::vec3u, T> ||
    std::same_as<math::vec3f, T> ||
    std::same_as<math::vec4i, T> ||
    std::same_as<math::vec4u, T> ||
    std::same_as<math::mat4f, T> ||
    std::same_as<math::vec4f, T>;

struct MaterialDetial {
    struct ParameterInfo {
        std::pmr::string name;
        std::type_index  type;
        std::size_t      offset;
        std::size_t      size;
    };
    std::pmr::string                          name;
    PrimitiveType                             primitive = PrimitiveType::TriangleList;
    std::shared_ptr<Shader>                   vertex_shader;
    std::shared_ptr<Shader>                   pixel_shader;
    std::pmr::vector<ParameterInfo>           parameters_info;
    std::pmr::unordered_set<std::pmr::string> texture_name;
};

class Material : public utils::enable_private_make_shared_build<Material>,
                 public std::enable_shared_from_this<Material>,
                 public MaterialDetial {
    friend class MaterialInstance;

public:
    class Builder : MaterialDetial {
        friend class Material;

    public:
        Builder& SetName(std::string_view name);
        Builder& SetPrimitive(PrimitiveType primitive);
        Builder& SetVertexShader(const std::filesystem::path& hlsl_path);
        Builder& SetPixelShader(const std::filesystem::path& hlsl_path);

        // Set rasizer state
        // Builder& Cull();
        // ...

        template <MaterialParametric T>
        Builder& AppendParameterInfo(std::string_view name, T default_value = {});

        template <MaterialParametric T>
        Builder& AppendParameterArrayInfo(std::string_view name, std::size_t count, std::pmr::vector<T> default_values = {});

        Builder& AppendTextureName(std::string_view name, std::filesystem::path default_value = {});

        std::shared_ptr<Material> Build();

    private:
        Builder& AppendParameterImpl(std::string_view name, const std::type_info& type_id, std::size_t size, std::byte* default_value);
        void     AddName(const std::pmr::string& name);

        std::pmr::unordered_set<std::pmr::string>                        exsisted_names;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> default_texture_paths;
        std::pmr::vector<std::byte>                                      default_buffer;
    };

    MaterialInstance CreateInstance() const noexcept;
    std::size_t      GetNumInstances() const noexcept;

    const auto& GetVertexShader() const noexcept { return vertex_shader; }
    const auto& GetPixelShader() const noexcept { return pixel_shader; }

    template <MaterialParametric T>
    bool IsValidParameter(std::string_view name) const noexcept;

    template <MaterialParametric T>
    bool IsValidParameterArray(std::string_view name, std::size_t count) const noexcept;

    std::optional<ParameterInfo>           GetParameterInfo(std::string_view name) const noexcept;
    const std::pmr::vector<ParameterInfo>& GetParameterInfos() const noexcept;
    // Get total parameters size
    std::size_t GetParametersSize() const noexcept;

    bool        IsValidTextureParameter(std::string_view name) const noexcept;
    const auto& GetTextureNames() const noexcept { return texture_name; }

    bool operator==(const Material& rhs) const;

protected:
    friend class Builder;
    Material(const Builder&);

private:
    void InitDefaultMaterialInstance(const Builder& buider);

    std::unique_ptr<MaterialInstance> m_DefaultInstance;
    std::size_t                       m_NumInstances = 0;
};

class MaterialInstance {
    friend class Material;

public:
    MaterialInstance() = default;
    MaterialInstance(const std::shared_ptr<Material>& material);

    MaterialInstance(const MaterialInstance& other);
    MaterialInstance(MaterialInstance&&) = default;

    MaterialInstance& operator=(const MaterialInstance&);
    MaterialInstance& operator=(MaterialInstance&&) = default;

    ~MaterialInstance();

    template <MaterialParametric T>
    MaterialInstance& SetParameter(std::string_view name, const T& value) noexcept;

    template <MaterialParametric T>
    MaterialInstance& SetParameter(std::string_view name, const std::pmr::vector<T> value) noexcept;

    // TODO set a sampler
    MaterialInstance& SetTexture(std::string_view name, std::shared_ptr<Texture> texture) noexcept;

    template <MaterialParametric T>
    std::optional<T> GetValue(std::string_view name) const noexcept;

    template <MaterialParametric T, std::size_t N>
    std::optional<std::array<T, N>> GetValue(std::string_view name) const noexcept;

    std::shared_ptr<Texture> GetTexture(std::string_view name) const noexcept;

    inline auto GetMaterial() const noexcept { return m_Material; }

    inline auto& GetParameterBuffer() const noexcept { return m_Parameters; }

    inline auto& GetTextures() const noexcept { return m_Textures; }

private:
    void Warn(std::string_view message) const;

    template <typename T>
    const T& GetParameter(std::string_view name) const noexcept;

    template <typename T>
    T& GetParameter(std::string_view name) noexcept;

    std::weak_ptr<Material> m_Material;

    core::Buffer                                                        m_Parameters;
    std::pmr::unordered_map<std::pmr::string, std::shared_ptr<Texture>> m_Textures;
};

template <MaterialParametric T>
auto Material::Builder::AppendParameterInfo(std::string_view name, T default_value) -> Builder& {
    return AppendParameterImpl(name, typeid(T), sizeof(T), reinterpret_cast<std::byte*>(&default_value));
}

template <MaterialParametric T>
auto Material::Builder::AppendParameterArrayInfo(std::string_view name, std::size_t count, std::pmr::vector<T> default_values) -> Builder& {
    default_values.resize(count);
    return AppendParameterImpl(name, typeid(T), sizeof(T) * count, reinterpret_cast<std::byte*>(default_values.data()));
}

template <MaterialParametric T>
bool Material::IsValidParameter(std::string_view name) const noexcept {
    auto iter = std::find_if(
        parameters_info.cbegin(),
        parameters_info.cend(),
        [&](const ParameterInfo& item) {
            if (name != item.name) return false;
            if (std::type_index(typeid(std::remove_cvref_t<T>)) == item.type)
                return true;
            return false;
        });

    return iter != parameters_info.cend();
}

template <MaterialParametric T>
bool Material::IsValidParameterArray(std::string_view name, std::size_t count) const noexcept {
    auto iter = std::find_if(
        parameters_info.cbegin(),
        parameters_info.cend(),
        [&](const ParameterInfo& item) {
            if (name != item.name) return false;
            if (std::type_index(typeid(std::remove_cvref_t<T>)) == item.type)
                return sizeof(T) * count == item.size;
            return false;
        });

    return iter != parameters_info.cend();
}

template <MaterialParametric T>
MaterialInstance& MaterialInstance::SetParameter(std::string_view name, const T& value) noexcept {
    auto material = m_Material.lock();

    if (material && material->IsValidParameter<T>(name)) {
        GetParameter<T>(name) = value;
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return *this;
}

template <MaterialParametric T>
MaterialInstance& MaterialInstance::SetParameter(std::string_view name, std::pmr::vector<T> values) noexcept {
    auto material = m_Material.lock();

    if (material && material->IsValidParameterArray<T>(name, values.size())) {
        std::copy(values.begin(), values.end(), &GetParameter<T>(name));
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return *this;
}

template <MaterialParametric T>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<T> {
    auto material = m_Material.lock();
    if (material && material->IsValidParameter<T>(name)) {
        return GetParameter<T>(name);
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }

    return std::nullopt;
}

template <MaterialParametric T, std::size_t N>
auto MaterialInstance::GetValue(std::string_view name) const noexcept -> std::optional<std::array<T, N>> {
    auto material = m_Material.lock();

    if (material && material->IsValidParameterArray<T>(name, N)) {
        std::array<T, N> result{};
        std::copy_n(&GetParameter<T>(name), N, result.data());
        return result;
    } else {
        Warn(fmt::format("You are setting a invalid parameter: {}", name));
    }
    return std::nullopt;
}

template <typename T>
const T& MaterialInstance::GetParameter(std::string_view name) const noexcept {
    auto material = m_Material.lock();
    auto info     = *material->GetParameterInfo(name);
    return *reinterpret_cast<const T*>(m_Parameters.GetData() + info.offset);
}

template <typename T>
T& MaterialInstance::GetParameter(std::string_view name) noexcept {
    return const_cast<T&>(const_cast<const MaterialInstance*>(this)->GetParameter<T>(name));
}

}  // namespace hitagi::resource