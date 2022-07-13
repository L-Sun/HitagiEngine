#pragma once
#include <filesystem>
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>
#include <hitagi/utils/private_build.hpp>

#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <bitset>

namespace hitagi::resource {

struct MaterialDetial {
    struct ParameterInfo {
        std::pmr::string name;
        std::type_index  type;
        std::size_t      offset;
        std::size_t      size;
    };
    PrimitiveType                             primitive;
    std::filesystem::path                     shader;
    std::pmr::vector<ParameterInfo>           parameters_info;
    std::pmr::unordered_set<std::pmr::string> texture_name;
};

class Material : public SceneObject,
                 public utils::enable_private_make_shared_build<Material>,
                 public std::enable_shared_from_this<Material>,
                 private MaterialDetial {
    friend class MaterialInstance;

public:
    class Builder : MaterialDetial {
        friend class Material;

    public:
        Builder& SetName(std::string_view name);

        Builder& SetPrimitive(PrimitiveType primitive);

        Builder& SetShader(const std::filesystem::path& path);

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

        std::pmr::string                                                 name;
        std::pmr::unordered_set<std::pmr::string>                        exsisted_names;
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> default_textures;
        std::pmr::vector<std::byte>                                      default_buffer;
    };

    std::shared_ptr<MaterialInstance> CreateInstance() const noexcept;
    std::size_t                       GetNumInstances() const noexcept;

    PrimitiveType GetPrimitiveType() const noexcept { return primitive; }

    const auto& GetShaderPath() const noexcept { return shader; }

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

    std::shared_ptr<MaterialInstance> m_DefaultInstance = nullptr;
    std::size_t                       m_NumInstances    = 0;
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

}  // namespace hitagi::resource