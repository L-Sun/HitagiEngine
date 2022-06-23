#pragma once
#include <crossguid/guid.hpp>
#include <memory_resource>

namespace hitagi::resource {
class SceneObject {
public:
    const xg::Guid&  GetGuid() const noexcept;
    void             SetName(std::string_view name) noexcept;
    std::string_view GetName() const noexcept;
    std::pmr::string GetUniqueName(std::string_view sep = "-") const noexcept;
    std::uint64_t    Version() const noexcept;
    void             IncreaseVersion() noexcept;

protected:
    explicit SceneObject();
    SceneObject(const SceneObject& obj);
    SceneObject(SceneObject&& obj) noexcept;

    SceneObject& operator=(const SceneObject& rhs);
    SceneObject& operator=(SceneObject&& rhs) = default;

    void RenewGuid() noexcept;

    xg::Guid         m_Guid;
    std::pmr::string m_Name;
    std::uint64_t    m_Version = 0;
};
}  // namespace hitagi::resource
