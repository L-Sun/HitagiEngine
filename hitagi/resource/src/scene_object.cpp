#include <hitagi/resource/scene_object.hpp>

#include <fmt/format.h>

namespace hitagi::resource {
SceneObject::SceneObject() : m_Guid(xg::newGuid()) {}

SceneObject::SceneObject(std::string_view name)
    : m_Guid(xg::newGuid()),
      m_Name(name) {}

SceneObject::SceneObject(const SceneObject& obj) : m_Guid(xg::newGuid()) {}

SceneObject& SceneObject::operator=(const SceneObject& rhs) {
    if (this != &rhs) {
        m_Name = rhs.m_Name;
    }
    return *this;
}

auto SceneObject::GetGuid() const noexcept -> const xg::Guid& { return m_Guid; }

void SceneObject::SetName(std::string_view name) noexcept { m_Name = std::pmr::string(name); }

auto SceneObject::GetName() const noexcept -> std::string_view { return m_Name; }

auto SceneObject::GetUniqueName(std::string_view sep) const noexcept -> std::string { return fmt::format("{}{}{}", m_Name, sep, m_Guid.str()); }

inline void SceneObject::RenewGuid() noexcept { m_Guid = xg::newGuid(); }

}  // namespace hitagi::resource