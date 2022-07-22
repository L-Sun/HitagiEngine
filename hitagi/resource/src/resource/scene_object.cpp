#include <hitagi/resource/scene_object.hpp>

#include <fmt/format.h>

namespace hitagi::resource {
ResourceObject::ResourceObject() : m_Guid(xg::newGuid()) {}

ResourceObject::ResourceObject(const ResourceObject& obj)
    : m_Guid(xg::newGuid()),
      m_Name(obj.m_Name) {}

ResourceObject::ResourceObject(ResourceObject&& obj) noexcept
    : m_Guid(obj.m_Guid),
      m_Name(std::move(obj.m_Name)) {}

ResourceObject& ResourceObject::operator=(const ResourceObject& rhs) {
    if (this != &rhs) {
        m_Name = rhs.m_Name;
    }
    return *this;
}

auto ResourceObject::GetGuid() const noexcept -> const xg::Guid& { return m_Guid; }

void ResourceObject::SetName(std::string_view name) noexcept { m_Name = name; }

auto ResourceObject::GetName() const noexcept -> std::string_view { return m_Name; }

auto ResourceObject::GetUniqueName(std::string_view sep) const noexcept -> std::pmr::string {
    return std::pmr::string{fmt::format("{}{}{}", m_Name, sep, m_Guid.str())};
}

std::uint64_t ResourceObject::Version() const noexcept { return m_Version; }

void ResourceObject::IncreaseVersion() noexcept { m_Version++; }

inline void ResourceObject::RenewGuid() noexcept { m_Guid = xg::newGuid(); }

}  // namespace hitagi::resource