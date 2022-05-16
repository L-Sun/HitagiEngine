#include <hitagi/resource/scene_object.hpp>

#include <fmt/format.h>

namespace hitagi::resource {
SceneObject::SceneObject(allocator_type alloc)
    : m_Guid(xg::newGuid()), m_Name(alloc) {}

SceneObject::SceneObject(const SceneObject& obj, allocator_type alloc)
    : m_Guid(xg::newGuid()),
      m_Name(obj.m_Name, alloc) {}

SceneObject& SceneObject::operator=(const SceneObject& rhs) {
    if (this != &rhs) {
        m_Name = rhs.m_Name;
    }
    return *this;
}

auto SceneObject::GetGuid() const noexcept -> const xg::Guid& { return m_Guid; }

void SceneObject::SetName(std::string_view name) noexcept { m_Name = name; }

auto SceneObject::GetName() const noexcept -> std::string_view { return m_Name; }

auto SceneObject::GetUniqueName(std::string_view sep) const noexcept -> std::string { return fmt::format("{}{}{}", m_Name, sep, m_Guid.str()); }

std::uint32_t SceneObject::Version() const noexcept { return m_Version; }

inline void SceneObject::RenewGuid() noexcept { m_Guid = xg::newGuid(); }

}  // namespace hitagi::resource