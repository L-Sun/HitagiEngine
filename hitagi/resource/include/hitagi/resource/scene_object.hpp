#pragma once
#include <crossguid/guid.hpp>
#include <memory_resource>

namespace hitagi::resource {
class SceneObject {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    const xg::Guid&  GetGuid() const noexcept;
    void             SetName(std::string_view name) noexcept;
    std::string_view GetName() const noexcept;
    std::string      GetUniqueName(std::string_view sep = "-") const noexcept;
    std::uint32_t    Version() const noexcept;

protected:
    SceneObject(allocator_type alloc);
    SceneObject(const SceneObject& obj, allocator_type alloc = {});
    SceneObject& operator=(const SceneObject& rhs);

    SceneObject(SceneObject&& rhs) noexcept            = default;
    SceneObject& operator=(SceneObject&& rhs) noexcept = default;

    void RenewGuid() noexcept;

    xg::Guid         m_Guid;
    std::pmr::string m_Name;
    std::uint32_t    m_Version = 0;
};
}  // namespace hitagi::resource
