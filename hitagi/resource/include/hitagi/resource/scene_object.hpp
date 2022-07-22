#pragma once
#include <crossguid/guid.hpp>
#include <memory_resource>

namespace hitagi::resource {
class ResourceObject {
public:
    const xg::Guid&  GetGuid() const noexcept;
    void             SetName(std::string_view name) noexcept;
    std::string_view GetName() const noexcept;
    std::pmr::string GetUniqueName(std::string_view sep = "-") const noexcept;
    std::uint64_t    Version() const noexcept;
    void             IncreaseVersion() noexcept;

protected:
    explicit ResourceObject();
    ResourceObject(const ResourceObject& obj);
    ResourceObject(ResourceObject&& obj) noexcept;

    ResourceObject& operator=(const ResourceObject& rhs);
    ResourceObject& operator=(ResourceObject&& rhs) = default;

    void RenewGuid() noexcept;

    xg::Guid         m_Guid;
    std::pmr::string m_Name;
    std::uint64_t    m_Version = 0;
};
}  // namespace hitagi::resource
