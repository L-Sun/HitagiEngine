#pragma once
#include <crossguid/guid.hpp>

#include <string>
#include <string_view>
#include <memory_resource>

namespace hitagi::asset {
class Resource {
public:
    Resource(std::string_view name = "", xg::Guid guid = {})
        : m_Name(name), m_Guid(guid.isValid() ? guid : xg::Guid()) {}

    Resource(const Resource&);
    Resource& operator=(const Resource&);

    Resource& operator=(Resource&&) = default;
    Resource(Resource&&)            = default;

    inline const auto& GetName() const noexcept { return m_Name; }
    inline const auto& GetGuid() const noexcept { return m_Guid; }
    auto               GetUniqueName() const noexcept -> std::pmr::string;

    inline void SetName(std::string_view name) noexcept { m_Name = name; }

protected:
    std::pmr::string m_Name;
    xg::Guid         m_Guid;
};
}  // namespace hitagi::asset