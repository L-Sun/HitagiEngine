#pragma once
#include <crossguid/guid.hpp>

#include <string>
#include <string_view>

namespace hitagi::asset {
class Resource {
public:
    enum struct Type {
        Scene,
        SceneNode,
        Vertex,
        Index,
        Mesh,
        Texture,
        Material,
        MaterialInstance,
        Camera,
        Light,
        Skeleton,
    };

    Resource(Type type, std::string_view name = "") : m_Type(type), m_Name(name), m_Guid(xg::Guid()) {}

    Resource(const Resource&);
    Resource& operator=(const Resource&);

    Resource& operator=(Resource&&) = default;
    Resource(Resource&&)            = default;

    inline auto        GetType() const noexcept { return m_Type; }
    inline auto        GetName() const noexcept -> std::string_view { return m_Name; }
    inline const auto& GetGuid() const noexcept { return m_Guid; }
    auto               GetUniqueName() const noexcept -> std::pmr::string;

    inline void SetName(std::string_view name) noexcept { m_Name = name; }

protected:
    Type             m_Type;
    std::pmr::string m_Name;
    xg::Guid         m_Guid;
};
}  // namespace hitagi::asset