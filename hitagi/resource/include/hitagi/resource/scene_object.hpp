#pragma once
#include <crossguid/guid.hpp>

namespace hitagi::resource {
class SceneObject {
public:
    const xg::Guid&  GetGuid() const noexcept;
    void             SetName(std::string_view name) noexcept;
    std::string_view GetName() const noexcept;
    std::string      GetUniqueName(std::string_view sep = "") const noexcept;

protected:
    SceneObject();
    SceneObject(std::string_view name);
    SceneObject(const SceneObject& obj);
    SceneObject& operator=(const SceneObject& rhs);

    SceneObject(SceneObject&& rhs) noexcept = default;
    SceneObject& operator=(SceneObject&& rhs) noexcept = default;

    void RenewGuid() noexcept;

    xg::Guid         m_Guid;
    std::pmr::string m_Name;
};
}  // namespace hitagi::resource
