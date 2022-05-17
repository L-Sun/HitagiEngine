#pragma once
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>
#include <hitagi/utils/private_build.hpp>

#include <memory>
#include <typeindex>
#include <typeinfo>
#include <memory_resource>
#include <unordered_set>

namespace hitagi::resource {

class Material : public SceneObject,
                 public utils::enable_private_make_shared_build<Material>,
                 public std::enable_shared_from_this<Material> {
    friend class MaterialInstance;

public:
    struct ParameterInfo {
        std::pmr::string name;
        std::type_index  type;
        std::size_t      offset;
        std::size_t      size;
    };

    class Builder {
        friend class Material;

    public:
        Builder& Type(MaterialType type) noexcept;

        template <MaterialParametric T>
        Builder& AppendParameterInfo(std::string_view name);

        template <MaterialParametric T, std::size_t N>
        Builder& AppendParameterArrayInfo(std::string_view name);

        Builder& AppendTextureName(std::string_view name);

        std::shared_ptr<Material> Build();

    private:
        Builder& AppendParameterImpl(std::string_view name, const std::type_info& type_id, std::size_t size);
        void     AddName(const std::pmr::string& name);

        MaterialType                              material_type;
        std::pmr::vector<ParameterInfo>           parameters_info;
        std::pmr::unordered_set<std::pmr::string> texture_name;

        std::pmr::unordered_set<std::pmr::string> exsisted_names;
    };

    std::shared_ptr<MaterialInstance> CreateInstance() const noexcept;
    std::size_t                       GetNumInstances() const noexcept;

    inline MaterialType GetType() const noexcept { return m_Type; }

    template <typename T>
    bool IsValidParameter(std::string_view name) const noexcept;

    bool IsValidTextureParameter(std::string_view name) const noexcept;

    std::optional<ParameterInfo> GetParameterInfo(std::string_view name) const noexcept;
    std::size_t                  GetParametersSize() const noexcept;

protected:
    friend class Builder;
    Material(const Builder&);

private:
    void InitDefaultMaterialInstance();

    MaterialType                              m_Type;
    std::pmr::vector<ParameterInfo>           m_ParametersInfo;
    std::pmr::unordered_set<std::pmr::string> m_ValidTextures;

    std::shared_ptr<MaterialInstance> m_DefaultInstance = nullptr;
    std::size_t                       m_NumInstances    = 0;
};

template <MaterialParametric T>
auto Material::Builder::AppendParameterInfo(std::string_view name) -> Builder& {
    return AppendParameterImpl(name, typeid(T), sizeof(T));
}

template <MaterialParametric T, std::size_t N>
auto Material::Builder::AppendParameterArrayInfo(std::string_view name) -> Builder& {
    return AppendParameterImpl(name, typeid(std::array<T, N>), sizeof(T) * N);
}

template <typename T>
bool Material::IsValidParameter(std::string_view name) const noexcept {
    auto iter = std::find_if(
        m_ParametersInfo.cbegin(),
        m_ParametersInfo.cend(),
        [&](const ParameterInfo& item) {
            return name == item.name && std::type_index(typeid(std::remove_cvref_t<T>)) == item.type;
        });

    return iter != m_ParametersInfo.cend();
}

}  // namespace hitagi::resource