#pragma once
#include <hitagi/utils/uuid.hpp>

#include <string>
#include <string_view>

namespace hitagi::asset {
class Resource {
public:
    enum struct Type : std::uint8_t {
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

    Resource(Type type, std::string_view name = "") : m_Type(type), m_Name(name), m_UUID(utils::UUID::Create()) {}

    Resource(const Resource&);
    Resource& operator=(const Resource&);

    Resource& operator=(Resource&&) = default;
    Resource(Resource&&)            = default;

    inline auto        GetType() const noexcept { return m_Type; }
    inline auto        GetName() const noexcept -> std::string_view { return m_Name; }
    inline const auto& GetUUID() const noexcept { return m_UUID; }
    auto               GetUniqueName() const noexcept -> std::pmr::string;

    inline void SetName(std::string_view name) noexcept { m_Name = name; }

protected:
    Type             m_Type;
    std::pmr::string m_Name;
    utils::UUID      m_UUID;
};
}  // namespace hitagi::asset